#include "tokenizer.h"
#include <cctype>
#include <unordered_map>

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"CREATE", TokenType::CREATE}, {"TABLE", TokenType::TABLE},
    {"INSERT", TokenType::INSERT}, {"INTO", TokenType::INTO},
    {"VALUES", TokenType::VALUES}, {"SELECT", TokenType::SELECT},
    {"FROM", TokenType::FROM},     {"WHERE", TokenType::WHERE},
    {"UPDATE", TokenType::UPDATE}, {"SET", TokenType::SET},
    {"DELETE", TokenType::DELETE}, {"AND", TokenType::AND},
    {"OR", TokenType::OR},         {"INT", TokenType::INT},
    {"TEXT", TokenType::TEXT},
};

static bool isDigit(char c)     { return std::isdigit(static_cast<unsigned char>(c)); }
static bool isLetter(char c)    { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
static bool isWordChar(char c)  { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;

    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];

        char next = (i + 1 < input.size()) ? input[i + 1] : '\0';

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;

        // Single-character symbols
        if (c == '*') { tokens.push_back({TokenType::STAR, "*"}); continue; }
        if (c == ',') { tokens.push_back({TokenType::COMMA, ","}); continue; }
        if (c == '(') { tokens.push_back({TokenType::LPAREN, "("}); continue; }
        if (c == ')') { tokens.push_back({TokenType::RPAREN, ")"}); continue; }
        if (c == ';') { tokens.push_back({TokenType::SEMICOLON, ";"}); continue; }
        if (c == '=') { tokens.push_back({TokenType::EQUAL, "="}); continue; }

        // Symbols that might be two characters: < <=  > >=  !=
        if (c == '<') {
            if (next == '=') { tokens.push_back({TokenType::LESS_EQUAL, "<="}); i++; }
            else             { tokens.push_back({TokenType::LESS, "<"}); }
            continue;
        }
        if (c == '>') {
            if (next == '=') { tokens.push_back({TokenType::GREATER_EQUAL, ">="}); i++; }
            else             { tokens.push_back({TokenType::GREATER, ">"}); }
            continue;
        }
        if (c == '!') {
            if (next == '=') { tokens.push_back({TokenType::NOT_EQUAL, "!="}); i++; }
            else             { tokens.push_back({TokenType::ERROR, "!"}); }
            continue;
        }

        // Numbers: 7, 2026, -5
        if (isDigit(c) || (c == '-' && isDigit(next))) {
            std::string buffer(1, c);
            while (i + 1 < input.size() && isDigit(input[i + 1])) {
                i++;
                buffer += input[i];
            }
            tokens.push_back({TokenType::NUMBER, buffer});
            continue;
        }

        
        if (isLetter(c)) {
            std::string buffer(1, c);
            while (i + 1 < input.size() && isWordChar(input[i + 1])) {
                i++;
                buffer += input[i];
            }

            std::string upper = buffer;
            for (char& ch : upper) ch = std::toupper(static_cast<unsigned char>(ch));

            auto found = KEYWORDS.find(upper);
            if (found != KEYWORDS.end()) tokens.push_back({found->second, buffer});
            else                         tokens.push_back({TokenType::IDENTIFIER, buffer});
            continue;
        }

        if (c == '\'') {
            std::string buffer;
            size_t j = i + 1;
            while (j < input.size() && input[j] != '\'') {
                buffer += input[j];
                j++;
            }
            if (j >= input.size()) {
                tokens.push_back({TokenType::ERROR, "'" + buffer});
                i = input.size();
                continue;
            }
            tokens.push_back({TokenType::STRING, buffer});
            i = j; 
            continue;
        }

        tokens.push_back({TokenType::ERROR, std::string(1, c)});
    }

    tokens.push_back({TokenType::END, ""});
    return tokens;
}

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::CREATE:        return "CREATE";
        case TokenType::TABLE:         return "TABLE";
        case TokenType::INSERT:        return "INSERT";
        case TokenType::INTO:          return "INTO";
        case TokenType::VALUES:        return "VALUES";
        case TokenType::SELECT:        return "SELECT";
        case TokenType::FROM:          return "FROM";
        case TokenType::WHERE:         return "WHERE";
        case TokenType::UPDATE:        return "UPDATE";
        case TokenType::SET:           return "SET";
        case TokenType::DELETE:        return "DELETE";
        case TokenType::AND:           return "AND";
        case TokenType::OR:            return "OR";
        case TokenType::INT:           return "INT";
        case TokenType::TEXT:          return "TEXT";
        case TokenType::IDENTIFIER:    return "IDENTIFIER";
        case TokenType::NUMBER:        return "NUMBER";
        case TokenType::STRING:        return "STRING";
        case TokenType::STAR:          return "STAR";
        case TokenType::COMMA:         return "COMMA";
        case TokenType::LPAREN:        return "LPAREN";
        case TokenType::RPAREN:        return "RPAREN";
        case TokenType::SEMICOLON:     return "SEMICOLON";
        case TokenType::EQUAL:         return "EQUAL";
        case TokenType::NOT_EQUAL:     return "NOT_EQUAL";
        case TokenType::LESS:          return "LESS";
        case TokenType::GREATER:       return "GREATER";
        case TokenType::LESS_EQUAL:    return "LESS_EQUAL";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::END:           return "END";
        case TokenType::ERROR:         return "ERROR";
    }
    return "UNKNOWN";
}