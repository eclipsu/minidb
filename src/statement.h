#pragma once
#include <string>
#include <vector>

enum class StatementType { CREATE };
enum class ValueType {INT, TEXT};

struct Column {
    ValueType type;
    std::string name;
};

struct Statement {
    StatementType type;
    std::string table;
    std::vector<Column> columns;
};

