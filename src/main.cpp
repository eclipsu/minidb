#include "executor.h"
#include "parser.h"
#include "tokenizer.h"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

// ---------- Line editing (arrow keys + history) ----------

static std::vector<std::string> history;
static termios origTerm;
static bool rawMode = false;

// Put the terminal back to normal. Runs on exit AND on Ctrl-C,
// so the terminal never gets stuck with echo turned off.
static void restoreTerminal() {
    if (rawMode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &origTerm);
        rawMode = false;
    }
}

static void onSignal(int sig) {
    restoreTerminal();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

static void enableRawMode() {
    if (!isatty(STDIN_FILENO)) return;   // piped input (e.g. a test script): no raw mode
    if (tcgetattr(STDIN_FILENO, &origTerm) != 0) return;
    termios raw = origTerm;
    raw.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return;
    rawMode = true;
    std::atexit(restoreTerminal);
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
}

static bool readLine(const char* prompt, std::string& buf) {
    // Not a terminal: just read a normal line.
    if (!rawMode) {
        std::cout << prompt << std::flush;
        return static_cast<bool>(std::getline(std::cin, buf));
    }

    buf.clear();
    size_t cur = history.size();
    size_t pos = 0;
    std::cout << prompt << std::flush;

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\n' || c == '\r') {
            std::cout << "\n";
            if (!buf.empty()) history.push_back(buf);
            return true;
        }
        if (c == 4 && buf.empty()) {          // Ctrl-D on an empty line
            std::cout << "\n";
            return false;
        }
        if (c == '\x1b') {                    // arrow keys: ESC [ A/B/C/D
            char s[2];
            if (read(STDIN_FILENO, s, 2) != 2) continue;
            if (s[0] == '[') {
                if (s[1] == 'A' && cur > 0) { buf = history[--cur]; pos = buf.size(); }
                if (s[1] == 'B' && cur < history.size()) {
                    buf = ++cur < history.size() ? history[cur] : "";
                    pos = buf.size();
                }
                if (s[1] == 'C' && pos < buf.size()) pos++;
                if (s[1] == 'D' && pos > 0) pos--;
                if (s[1] == 'H') pos = 0;              // Home
                if (s[1] == 'F') pos = buf.size();     // End
                if (s[1] == '3') {                     // Delete key: ESC [ 3 ~
                    char tilde;
                    if (read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~' && pos < buf.size())
                        buf.erase(pos, 1);
                }
            }
        } else if (c == 127 || c == 8) {      // Backspace
            if (pos > 0) buf.erase(--pos, 1);
        } else if (c >= 32) {
            buf.insert(pos++, 1, c);
        }

        std::cout << "\r\x1b[K" << prompt << buf;
        if (pos < buf.size()) std::cout << "\x1b[" << buf.size() - pos << "D";
        std::cout << std::flush;
    }
    return false;
}

// ---------- Dot commands ----------

static std::string dataDir = "data";   // where .save / .load go, and what .exit saves to

// ".save" -> "data",  ".save backup" -> "backup" (and remember it)
static std::string dirArgument(const std::string& line) {
    std::istringstream in(line);
    std::string command, dir;
    in >> command >> dir;
    if (!dir.empty()) dataDir = dir;
    return dataDir;
}

static void printTables(const Database& db) {
    std::vector<std::string> names = db.tableNames();
    if (names.empty()) std::cout << "(no tables)\n";
    for (const std::string& name : names)
        std::cout << name << " (" << db.getTable(name)->rows.size() << " rows)\n";
}

// Asks what to do with unsaved changes. Returns true if it's OK to quit.
static bool confirmExit(Database& db) {
    if (!db.hasUnsavedChanges()) return true;
    while (true) {
        std::string answer;
        std::string prompt = "You have unsaved changes. save (to " + dataDir + "/), discard or cancel? ";
        if (!readLine(prompt.c_str(), answer)) return true;
        if (answer == "save" || answer == "s") {
            Result r = db.saveAll(dataDir);
            std::cout << formatResult(r) << "\n";
            return r.ok;
        }
        if (answer == "discard" || answer == "d") return true;
        if (answer == "cancel" || answer == "c") return false;
    }
}

// ---------- REPL ----------

int main() {
    enableRawMode();

    Database db;
    std::string line;
    while (true) {
        if (!readLine("db > ", line)) {        // Ctrl-D or end of piped input
            if (db.hasUnsavedChanges()) std::cout << "(unsaved changes were not saved)\n";
            break;
        }
        if (line.empty()) continue;

        if (line == ".exit") {
            if (confirmExit(db)) break;
            continue;
        }
        if (line == ".tables") {
            printTables(db);
            continue;
        }
        if (line == ".save" || line.rfind(".save ", 0) == 0) {
            std::cout << formatResult(db.saveAll(dirArgument(line))) << "\n";
            continue;
        }
        if (line == ".load" || line.rfind(".load ", 0) == 0) {
            std::string dir = dirArgument(line);
            if (db.hasUnsavedChanges()) {
                std::string answer;
                readLine("Loading throws away your unsaved changes. Continue? (y/n) ", answer);
                if (answer != "y" && answer != "yes") continue;
            }
            std::cout << formatResult(db.loadAll(dir)) << "\n";
            continue;
        }
        if (line[0] == '.') {
            std::cout << "Unknown command " << line << " (try .tables, .save [dir], .load [dir], .exit)\n";
            continue;
        }

        try {
            Statement stmt = parse(tokenize(line));
            std::cout << formatResult(db.execute(stmt)) << "\n";
        } catch (const ParseError& e) {
            std::cout << "Syntax error: " << e.what() << "\n";
        }
    }

    restoreTerminal();
}
