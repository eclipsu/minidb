#pragma once
#include <string>
#include <vector>

enum class StatementType { CREATE, INSERT };
enum class ValueType {INT, TEXT};

struct Column {
    ValueType type;
    std::string name;
};

struct Value {
    ValueType type;
    std::string text;
};

struct Statement {
    StatementType type;
    std::string table;
    std::vector<Column> columns;
    std::vector<Value> values;
};

