#include "parser.h"
#include "tokenizer.h"
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> history;

static bool readLine(const char* prompt, std::string& buf) {
    buf.clear();
    size_t cur = history.size();
    size_t pos = 0;
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
            if (s[1] == 'A' && cur > 0) { buf = history[--cur]; pos = buf.size(); }
            if (s[1] == 'B' && cur < history.size()) {
                buf = ++cur < history.size() ? history[cur] : "";
                pos = buf.size();
            }
            if (s[1] == 'C' && pos < buf.size()) pos++;
            if (s[1] == 'D' && pos > 0) pos--;
        } else if (c == 127) {
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
    std::string value = e->value.type == ValueType::TEXT ? "'" + e->value.text + "'" : e->value.text;
    return "(" + e->column + " " + op + " " + value + ")";
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
                for (const Value& v : stmt.values)
                    std::cout << " " << (v.type == ValueType::INT ? v.text : "'" + v.text + "'");
            } else if (stmt.type == StatementType::SELECT) {
                std::cout << "Parsed SELECT from " << stmt.table << ", columns:";
                if (stmt.selectColumns.empty()) std::cout << " *";
                for (const std::string& c : stmt.selectColumns) std::cout << " " << c;
                if (stmt.where) std::cout << ", WHERE " << whereToString(stmt.where.get());
            }
            std::cout << "\n";
        } catch (const ParseError& e) {
            std::cout << "Syntax error: " << e.what() << "\n";
        }
    }

    tcsetattr(0, TCSANOW, &orig);
}