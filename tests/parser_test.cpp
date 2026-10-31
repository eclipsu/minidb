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

static std::string valuesToString(const Statement& s) {
    std::string out;
    for (size_t i = 0; i < s.values.size(); i++) {
        if (i > 0) out += ", ";
        out += (s.values[i].type == ValueType::INT) ? "INT " : "TEXT ";
        out += s.values[i].text;
    }
    return out;
}

static void expectInsert(const std::string& name, const std::string& sql,
                         const std::string& table, const std::string& values) {
    try {
        Statement s = parse(tokenize(sql));
        if (s.type != StatementType::INSERT)
            return fail(name, "type should be INSERT");
        if (s.table != table)
            return fail(name, "table should be '" + table + "' but was '" + s.table + "'");
        if (valuesToString(s) != values)
            return fail(name, "values should be [" + values + "] but were [" + valuesToString(s) + "]");
        pass(name);
    } catch (const ParseError& e) {
        fail(name, std::string("should have worked, but got error: ") + e.what());
    }
}

// The command should be rejected with a ParseError.
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

    std::cout << "\n-- Lesson 3: valid INSERT --\n";
    expectInsert("one number",        "INSERT INTO t VALUES (7)",                    "t",     "INT 7");
    expectInsert("one string",        "INSERT INTO t VALUES ('bob')",                "t",     "TEXT bob");
    expectInsert("mixed values",      "INSERT INTO users VALUES (1, 'bob', 20);",    "users", "INT 1, TEXT bob, INT 20");
    expectInsert("negative number",   "INSERT INTO t VALUES (-5)",                   "t",     "INT -5");
    expectInsert("string with spaces","INSERT INTO t VALUES ('hello world')",        "t",     "TEXT hello world");
    expectInsert("lowercase",         "insert into pets values (1, 'rex')",          "pets",  "INT 1, TEXT rex");

    std::cout << "\n-- Lesson 3: broken INSERT --\n";
    expectError("missing INTO",              "INSERT users VALUES (1)");
    expectError("missing table name",        "INSERT INTO VALUES (1)");
    expectError("missing VALUES",            "INSERT INTO users (1)");
    expectError("missing (",                 "INSERT INTO users VALUES 1)");
    expectError("missing )",                 "INSERT INTO users VALUES (1, 2");
    expectError("empty ()",                  "INSERT INTO users VALUES ()");
    expectError("trailing comma",            "INSERT INTO users VALUES (1,)");
    expectError("missing comma",             "INSERT INTO users VALUES (1 2)");
    expectError("column name, not a value",  "INSERT INTO users VALUES (id)");
    expectError("extra word after ;",        "INSERT INTO users VALUES (1); extra");
    expectError("unclosed string",           "INSERT INTO users VALUES ('bob)");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}