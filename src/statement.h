#pragma once
#include "tokenizer.h"
#include <string>
#include <vector>
#include <memory>

enum class StatementType { CREATE, INSERT, SELECT };
enum class ValueType { INT, TEXT };
enum ExprKind { COMPARE, AND, OR };

struct Column {
    ValueType type;
    std::string name;
};

struct Value {
    ValueType type;
    std::string text;
};

struct Expr {
    ExprKind kind = ExprKind::COMPARE;
    std::string column;
    TokenType op = TokenType::EQUAL;
    Value value;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct Statement {
    StatementType type;
    std::string table;
    std::vector<Column> columns;
    std::vector<Value> values;
    std::vector<std::string> selectColumns;
    std::unique_ptr<Expr> where;
};

