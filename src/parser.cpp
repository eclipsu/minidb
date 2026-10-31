#include "parser.h"

class Parser {
public:
    Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

    Statement parseStatement() {
        if (peek().type == TokenType::CREATE) {
            return parseCreate();
        } if (peek().type == TokenType::INSERT) {
            return parseInsert();
        }
         else {
            throw ParseError("Unknown command");
        }
    }

private:
    const std::vector<Token>& tokens;
    size_t pos = 0;

    const Token& peek() {
        return tokens[pos];
    }

    const Token& expect(TokenType type, const std::string& what) {
        if (peek().type != type) {
            throw ParseError("Expected " + what + " but found '" + peek().text + "'");
        }
        return tokens[pos++];
    }

    bool match(TokenType type) {
        if (peek().type != type) return false;
        pos++;
        return true;
    }

    Statement parseCreate() {
        Statement s;

        s.type = StatementType::CREATE;
        

        expect(TokenType::CREATE, "CREATE");
        expect(TokenType::TABLE, "TABLE");
        
        s.table = expect(TokenType::IDENTIFIER, "table name").text;
        expect(TokenType::LPAREN, "(");
        
        do {
            s.columns.push_back(parseColumn());
        } while(match(TokenType::COMMA));

        expect(TokenType::RPAREN, ")");
        match(TokenType::SEMICOLON);
        expect(TokenType::END, "end of command");
        return s;
    }

    Column parseColumn() {
        Column col;
        col.name = expect(TokenType::IDENTIFIER, "column name").text;
        
        if(match(TokenType::INT)) {
            col.type = ValueType::INT;
        } else if (match(TokenType::TEXT)) {
            col.type = ValueType::TEXT;
        } else {
            throw ParseError("Expected Int or Text");
        }
        return col;
    }

    Statement parseInsert() {
        Statement s;
        s.type = StatementType::INSERT;

        expect(TokenType::INSERT, "INSERT");
        expect(TokenType::INTO, "INTO");
        s.table = expect(TokenType::IDENTIFIER, "table name").text;
        expect(TokenType::VALUES, "VALUES");
        expect(TokenType::LPAREN, "(");
        do {
            s.values.push_back(parseValue());
        } while(match(TokenType::COMMA));
        expect(TokenType::RPAREN, ")");
        match(TokenType::SEMICOLON);
        expect(TokenType::END, "end");

        return s;
    }
 
    Value parseValue() {
        Value v;
 
        if (peek().type == TokenType::NUMBER) {
            v.type = ValueType::INT;
            v.text = expect(TokenType::NUMBER, "number").text;
        } else if (peek().type == TokenType::STRING) {
            v.type = ValueType::TEXT;
            v.text = expect(TokenType::STRING, "string").text;
        } else {
            throw ParseError("Expected a number or 'text'");
        }
        return v;
    }

};

Statement parse(const std::vector<Token>& tokens) {
    Parser parser(tokens);
    return parser.parseStatement();
}