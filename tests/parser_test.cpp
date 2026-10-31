#include "../src/parser.h"
#include "../src/tokenizer.h"
#include <iostream>
#include <string>

static int passed = 0;
static int failed = 0;

static void pass(const std::string& name) {
    passed++;
    std::cout << "  PASS  " << name << "\n";
}

static void fail(const std::string& name, const std::string& why) {
    failed++;
    std::cout << "  FAIL  " << name << "\n        " << why << "\n";
}

static std::string columnsToString(const Statement& s) {
    std::string out;
    for (size_t i = 0; i < s.columns.size(); i++) {
        if (i > 0) out += ", ";
        out += s.columns[i].name;
        out += (s.columns[i].type == ValueType::INT) ? " INT" : " TEXT";
    }
    return out;
}

static void expectCreate(const std::string& name, const std::string& sql,
                         const std::string& table, const std::string& columns) {
    try {
        Statement s = parse(tokenize(sql));
        if (s.table != table)
            return fail(name, "table should be '" + table + "' but was '" + s.table + "'");
        if (columnsToString(s) != columns)
            return fail(name, "columns should be [" + columns + "] but were [" + columnsToString(s) + "]");
        pass(name);
    } catch (const ParseError& e) {
        fail(name, std::string("should have worked, but got error: ") + e.what());
    }
}

static void expectError(const std::string& name, const std::string& sql) {
    try {
        parse(tokenize(sql));
        fail(name, "should have been an error: " + sql);
    } catch (const ParseError&) {
        pass(name);
    }
}

int main() {
    std::cout << "\n-- Lesson 2: valid CREATE TABLE --\n";
    expectCreate("one column",        "CREATE TABLE t (x INT)",                          "t",     "x INT");
    expectCreate("one TEXT column",   "CREATE TABLE t (x TEXT)",                         "t",     "x TEXT");
    expectCreate("three columns",     "CREATE TABLE users (id INT, name TEXT, age INT)", "users", "id INT, name TEXT, age INT");
    expectCreate("with semicolon",    "CREATE TABLE users (id INT);",                    "users", "id INT");
    expectCreate("lowercase",         "create table pets (name text)",                   "pets",  "name TEXT");

    std::cout << "\n-- Lesson 2: broken CREATE TABLE --\n";
    expectError("no columns at all",        "CREATE TABLE users");
    expectError("missing (",                "CREATE TABLE users id INT)");
    expectError("missing )",                "CREATE TABLE users (id INT");
    expectError("empty ()",                 "CREATE TABLE users ()");
    expectError("missing column type",      "CREATE TABLE users (id)");
    expectError("bad column type",          "CREATE TABLE users (id users)");
    expectError("trailing comma",           "CREATE TABLE users (id INT,)");
    expectError("missing comma",            "CREATE TABLE users (id INT name TEXT)");
    expectError("extra word after ;",       "CREATE TABLE users (id INT); extra");
    expectError("not a CREATE command",     "SELECT * FROM users");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}