#include "parser.h"
#include "tokenizer.h"
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> hist;

bool readLine(const char* prompt, std::string& buf) {
    buf.clear();
    size_t cur = hist.size();
    std::cout << prompt << std::flush;

    char c;
    while (read(0, &c, 1) == 1) {
        if (c == '\n') {
            std::cout << "\n";
            if (!buf.empty()) hist.push_back(buf);
            return true;
        }
        if (c == 4 && buf.empty()) return false;
        if (c == '\x1b') {
            char s[2];
            if (read(0, s, 2) != 2) continue;
            if (s[1] == 'A' && cur > 0) buf = hist[--cur];
            if (s[1] == 'B' && cur < hist.size()) buf = ++cur < hist.size() ? hist[cur] : "";
        } else if (c == 127) {
            if (!buf.empty()) buf.pop_back();
        } else if (c >= 32) {
            buf += c;
        }
        std::cout << "\r\x1b[K" << prompt << buf << std::flush;
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

            if (stmt.type == StatementType::CREATE) {
                std::cout << "Parsed CREATE TABLE " << stmt.table << " with columns:";
                for (const Column& c : stmt.columns)
                    std::cout << " " << c.name << (c.type == ValueType::INT ? "(INT)" : "(TEXT)");
            } else if (stmt.type == StatementType::INSERT) {
                std::cout << "Parsed INSERT INTO " << stmt.table << " with values:";
                for (const Value& v : stmt.values) {
                    if (v.type == ValueType::INT) std::cout << " " << v.text;
                    else                          std::cout << " '" << v.text << "'";
                }
            }
            std::cout << "\n";
        } catch (const ParseError& e) {
            std::cout << "Syntax error: " << e.what() << "\n";
        }
    }

    tcsetattr(0, TCSANOW, &orig);
}