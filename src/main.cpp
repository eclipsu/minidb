#include "tokenizer.h"
#include <iostream>
#include <string>

// For now the REPL just prints tokens so you can try the tokenizer by hand.
// Later this will call the parser and executor instead.
int main() {
    std::string line;
    while (true) {
        std::cout << "db > ";
        if (!std::getline(std::cin, line)) break;
        if (line == ".exit" || line == "q") break;

        for (const Token& t : tokenize(line)) {
            std::cout << tokenTypeName(t.type);
            if (t.type == TokenType::IDENTIFIER || t.type == TokenType::NUMBER ||
                t.type == TokenType::STRING)
                std::cout << "(" << t.text << ")";
            std::cout << " ";
        }
        std::cout << "\n";
    }
}
