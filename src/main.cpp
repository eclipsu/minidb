#include "parser.h"
#include "tokenizer.h"
#include <iostream>
#include <string>

int main() {
    std::string line;
    while (true) {
        std::cout << "db > ";
        if (!std::getline(std::cin, line)) break;
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
}