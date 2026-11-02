#include "parser.h"
#include "tokenizer.h"
#include <csignal>
#include <cstdlib>
#include <iostream>
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

// ---------- Printing what the parser understood ----------

static std::string valueToString(const Value& v) {
    return v.type == ValueType::TEXT ? "'" + v.text + "'" : v.text;
}

static std::string whereToString(const Expr* e) {
    if (!e) return "";
    if (e->kind == ExprKind::AND || e->kind == ExprKind::OR) {
        std::string op = e->kind == ExprKind::AND ? " AND " : " OR ";
        return "(" + whereToString(e->left.get()) + op + whereToString(e->right.get()) + ")";
    }
    std::string op;
    switch (e->op) {
        case TokenType::EQUAL:         op = "=";  break;
        case TokenType::NOT_EQUAL:     op = "!="; break;
        case TokenType::LESS:          op = "<";  break;
        case TokenType::GREATER:       op = ">";  break;
        case TokenType::LESS_EQUAL:    op = "<="; break;
        case TokenType::GREATER_EQUAL: op = ">="; break;
        default:                       op = "?";  break;
    }
    return "(" + e->column + " " + op + " " + valueToString(e->value) + ")";
}

static void printStatement(const Statement& stmt) {
    switch (stmt.type) {
        case StatementType::CREATE:
            std::cout << "Parsed CREATE TABLE " << stmt.table << " with columns:";
            for (const Column& c : stmt.columns)
                std::cout << " " << c.name << (c.type == ValueType::INT ? "(INT)" : "(TEXT)");
            break;

        case StatementType::INSERT:
            std::cout << "Parsed INSERT INTO " << stmt.table << " with values:";
            for (const Value& v : stmt.values) std::cout << " " << valueToString(v);
            break;

        case StatementType::SELECT:
            std::cout << "Parsed SELECT from " << stmt.table << ", columns:";
            if (stmt.selectColumns.empty()) std::cout << " *";
            for (const std::string& c : stmt.selectColumns) std::cout << " " << c;
            if (stmt.where) std::cout << ", WHERE " << whereToString(stmt.where.get());
            break;

    }
    std::cout << "\n";
}

// ---------- REPL ----------

int main() {
    enableRawMode();

    std::string line;
    while (readLine("db > ", line)) {
        if (line == ".exit") break;
        if (line.empty()) continue;

        try {
            Statement stmt = parse(tokenize(line));
            printStatement(stmt);
        } catch (const ParseError& e) {
            std::cout << "Syntax error: " << e.what() << "\n";
        }
    }

    restoreTerminal();
}