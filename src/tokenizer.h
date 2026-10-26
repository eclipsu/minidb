#pragma once
#include <string>
#include <vector>

enum class TokenType {
    // keywords
    CREATE, TABLE, INSERT, INTO, VALUES, SELECT, FROM, WHERE,
    UPDATE, SET, DELETE, AND, OR, INT, TEXT,
    // values
    IDENTIFIER, NUMBER, STRING,
    // symbols
    STAR, COMMA, LPAREN, RPAREN, SEMICOLON,
    EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
    // special
    END, ERROR
};

struct Token {
    TokenType type;
    std::string text;  // IDENTIFIER: the name, NUMBER: the digits,
                       // STRING: contents WITHOUT quotes, others: anything
};

// Turns one SQL command into tokens. The last token is always END.
std::vector<Token> tokenize(const std::string& input);

// For printing tokens in tests and debugging, e.g. "GREATER_EQUAL".
std::string tokenTypeName(TokenType type);
