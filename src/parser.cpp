#include "parser.h"

class Parser {
public:
    Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

    Statement parseStatement() {
        if (peek().type == TokenType::CREATE) {
            return parseCreate();
        } else if (peek().type == TokenType::INSERT) {
            return parseInsert();
        } else if (peek().type == TokenType::SELECT) {
            return parseSelect();
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
 
    Statement parseSelect() {
        Statement s;
        s.type = StatementType::SELECT;

        expect(TokenType::SELECT, "SELECT");
        if (match(TokenType::STAR)) {} else {
             do {
               s.selectColumns.push_back(expect(TokenType::IDENTIFIER, "column names").text);
            } while (match(TokenType::COMMA));
        }
        
        expect(TokenType::FROM, "FROM");
        s.table = expect(TokenType::IDENTIFIER, "table name").text;
        
        if (match(TokenType::WHERE)) {
            s.where = parseWhere();
        }

        match(TokenType::SEMICOLON);
        expect(TokenType::END, "end");
 
        return s;
    }

    std::unique_ptr<Expr> parseWhere() {
        std::unique_ptr<Expr> left = parseCondition();
        while (match(TokenType::AND)) {
            std::unique_ptr<Expr> right = parseCondition();
            left = makeNode(ExprKind::AND, std::move(left), std::move(right));
        }

        return left;
    }

    std::unique_ptr<Expr> makeNode(ExprKind kind, std::unique_ptr<Expr> left,
                                   std::unique_ptr<Expr> right) {
        std::unique_ptr<Expr> node = std::make_unique<Expr>();
        node->kind = kind;
        node->left = std::move(left);
        node->right = std::move(right);
        return node;
    }

    std::unique_ptr<Expr> parseCondition() {

        std::unique_ptr<Expr> expr = std::make_unique<Expr>(); 
        expr->column = expect(TokenType::IDENTIFIER, "column name").text; 
        if (isOperator(peek().type)) {
            expr->op = peek().type;
            pos++;
        } else {
            throw ParseError("Invalid Operated, Expected =, !=, <, >, <= or >=");
        }
        expr->value = parseValue();
 
        return expr;
    }
 
    bool isOperator(TokenType t) {
        return t == TokenType::EQUAL      || t == TokenType::NOT_EQUAL ||
               t == TokenType::LESS       || t == TokenType::GREATER   ||
               t == TokenType::LESS_EQUAL || t == TokenType::GREATER_EQUAL;
    }

};

Statement parse(const std::vector<Token>& tokens) {
    Parser parser(tokens);
    return parser.parseStatement();
}