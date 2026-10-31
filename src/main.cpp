#include "parser.h"
#include "tokenizer.h"
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> history;

bool readLine(const char* prompt, std::string& buf) {
    buf.clear();
    size_t cur = history.size();
    std::cout << prompt << std::flush;

    char c;
    while (read(0, &c, 1) == 1) {
        if (c == '\n') {
            std::cout << "\n";
            if (!buf.empty()) history.push_back(buf);
            return true;
        }
        if (c == 4 && buf.empty()) return false;
        if (c == '\x1b') {
            char s[2];
            if (read(0, s, 2) != 2) continue;
            if (s[1] == 'A' && cur > 0) buf = history[--cur];
            if (s[1] == 'B' && cur < history.size()) buf = ++cur < history.size() ? history[cur] : "";
        } else if (c == 127) {
            if (!buf.empty()) buf.pop_back();
        } else if (c >= 32) {
            buf += c;
        }
        std::cout << "\r\x1b[K" << prompt << buf << std::flush;  // redraw
    }
    return false;
}

int main() {
    termios orig, raw;
    tcgetattr(0, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);

    std::string line;
    while (readLine("db > ", line)) {
        if (line == ".exit") break;
        if (line.empty()) continue;

        try {
            Statement stmt = parse(tokenize(line));
            std::cout << "Parsed CREATE TABLE " << stmt.table << " with columns:";
            for (const Column& c : stmt.columns)
                std::cout << " " << c.name << (c.type == ValueType::INT ? "(INT)" : "(TEXT)");
            std::cout << "\n";
        } catch (const ParseError& e) {
            std::cout << "Syntax error: " << e.what() << "\n";
        }
    }

    tcsetattr(0, TCSANOW, &orig);
}