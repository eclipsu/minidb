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

static std::string selectToString(const Statement& s) {
    if (s.selectColumns.empty()) return "*";
    std::string out;
    for (size_t i = 0; i < s.selectColumns.size(); i++) {
        if (i > 0) out += ", ";
        out += s.selectColumns[i];
    }
    return out;
}

static void expectSelect(const std::string& name, const std::string& sql,
                         const std::string& table, const std::string& columns) {
    try {
        Statement s = parse(tokenize(sql));
        if (s.type != StatementType::SELECT)
            return fail(name, "type should be SELECT");
        if (s.table != table)
            return fail(name, "table should be '" + table + "' but was '" + s.table + "'");
        if (selectToString(s) != columns)
            return fail(name, "columns should be [" + columns + "] but were [" + selectToString(s) + "]");
        pass(name);
    } catch (const ParseError& e) {
        fail(name, std::string("should have worked, but got error: ") + e.what());
    }
}

static std::string whereToString(const Expr* e) {
    if (e == nullptr) return "";
    if (e->kind == ExprKind::AND)
        return "(" + whereToString(e->left.get()) + " AND " + whereToString(e->right.get()) + ")";
    if (e->kind == ExprKind::OR)
        return "(" + whereToString(e->left.get()) + " OR " + whereToString(e->right.get()) + ")";
    std::string op;
    switch (e->op) {
        case TokenType::EQUAL:         op = "=";  break;
        case TokenType::NOT_EQUAL:     op = "!="; break;
        case TokenType::LESS:          op = "<";  break;
        case TokenType::GREATER:       op = ">";  break;
        case TokenType::LESS_EQUAL:    op = "<="; break;
        case TokenType::GREATER_EQUAL: op = ">="; break;
        default:                       op = "?";  break;
    }
    std::string value = (e->value.type == ValueType::TEXT) ? "'" + e->value.text + "'" : e->value.text;
    return "(" + e->column + " " + op + " " + value + ")";
}

static void expectWhere(const std::string& name, const std::string& sql,
                        const std::string& table, const std::string& where) {
    try {
        Statement s = parse(tokenize(sql));
        if (s.type != StatementType::SELECT)
            return fail(name, "type should be SELECT");
        if (s.table != table)
            return fail(name, "table should be '" + table + "' but was '" + s.table + "'");
        std::string actual = whereToString(s.where.get());
        if (actual != where)
            return fail(name, "WHERE should be [" + where + "] but was [" + actual + "]");
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
    std::cout << "\nvalid CREATE TABLE --\n";
    expectCreate("one column",        "CREATE TABLE t (x INT)",                          "t",     "x INT");
    expectCreate("one TEXT column",   "CREATE TABLE t (x TEXT)",                         "t",     "x TEXT");
    expectCreate("three columns",     "CREATE TABLE users (id INT, name TEXT, age INT)", "users", "id INT, name TEXT, age INT");
    expectCreate("with semicolon",    "CREATE TABLE users (id INT);",                    "users", "id INT");
    expectCreate("lowercase",         "create table pets (name text)",                   "pets",  "name TEXT");

    std::cout << "\nbroken CREATE TABLE --\n";
    expectError("no columns at all",        "CREATE TABLE users");
    expectError("missing (",                "CREATE TABLE users id INT)");
    expectError("missing )",                "CREATE TABLE users (id INT");
    expectError("empty ()",                 "CREATE TABLE users ()");
    expectError("missing column type",      "CREATE TABLE users (id)");
    expectError("bad column type",          "CREATE TABLE users (id users)");
    expectError("trailing comma",           "CREATE TABLE users (id INT,)");
    expectError("missing comma",            "CREATE TABLE users (id INT name TEXT)");
    expectError("extra word after ;",       "CREATE TABLE users (id INT); extra");
    expectError("unknown command",          "DROP TABLE users");

    std::cout << "\nvalid INSERT --\n";
    expectInsert("one number",        "INSERT INTO t VALUES (7)",                    "t",     "INT 7");
    expectInsert("one string",        "INSERT INTO t VALUES ('bob')",                "t",     "TEXT bob");
    expectInsert("mixed values",      "INSERT INTO users VALUES (1, 'bob', 20);",    "users", "INT 1, TEXT bob, INT 20");
    expectInsert("negative number",   "INSERT INTO t VALUES (-5)",                   "t",     "INT -5");
    expectInsert("string with spaces","INSERT INTO t VALUES ('hello world')",        "t",     "TEXT hello world");
    expectInsert("lowercase",         "insert into pets values (1, 'rex')",          "pets",  "INT 1, TEXT rex");

    std::cout << "\nbroken INSERT --\n";
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

    std::cout << "\nvalid SELECT --\n";
    expectSelect("star",             "SELECT * FROM users",              "users", "*");
    expectSelect("one column",       "SELECT name FROM users",           "users", "name");
    expectSelect("two columns",      "SELECT name, age FROM users",      "users", "name, age");
    expectSelect("with semicolon",   "SELECT * FROM users;",             "users", "*");
    expectSelect("lowercase",        "select id from pets",              "pets",  "id");

    std::cout << "\nbroken SELECT --\n";
    expectError("missing columns",          "SELECT FROM users");
    expectError("missing FROM",             "SELECT * users");
    expectError("missing table name",       "SELECT * FROM");
    expectError("trailing comma",           "SELECT name, FROM users");
    expectError("missing comma",            "SELECT name age FROM users");
    expectError("star and a column",        "SELECT *, name FROM users");
    expectError("extra word at end",        "SELECT * FROM users users");

    std::cout << "\nvalid WHERE --\n";
    expectWhere("no WHERE means nullptr",  "SELECT * FROM users",                       "users", "");
    expectWhere("greater than",            "SELECT * FROM users WHERE age > 18",        "users", "(age > 18)");
    expectWhere("equal",                   "SELECT * FROM users WHERE id = 1",          "users", "(id = 1)");
    expectWhere("not equal",               "SELECT * FROM users WHERE id != 1",         "users", "(id != 1)");
    expectWhere("less than",               "SELECT * FROM users WHERE age < 18",        "users", "(age < 18)");
    expectWhere("less or equal",           "SELECT * FROM users WHERE age <= 18",       "users", "(age <= 18)");
    expectWhere("greater or equal",        "SELECT * FROM users WHERE age >= 18",       "users", "(age >= 18)");
    expectWhere("text value",              "SELECT * FROM users WHERE name = 'bob'",    "users", "(name = 'bob')");
    expectWhere("negative number",         "SELECT * FROM t WHERE x > -5",              "t",     "(x > -5)");
    expectWhere("with semicolon",          "SELECT * FROM users WHERE id = 1;",         "users", "(id = 1)");
    expectWhere("columns and WHERE",       "SELECT name, age FROM users WHERE id = 1",  "users", "(id = 1)");
    expectWhere("lowercase",               "select * from pets where name = 'rex'",     "pets",  "(name = 'rex')");

    std::cout << "\nbroken WHERE --\n";
    expectError("WHERE with nothing after",   "SELECT * FROM users WHERE");
    expectError("missing operator",           "SELECT * FROM users WHERE age 18");
    expectError("missing value",              "SELECT * FROM users WHERE age >");
    expectError("value on the left",          "SELECT * FROM users WHERE 18 < age");
    expectError("column on the right",        "SELECT * FROM users WHERE age > id");
    expectError("not an operator",            "SELECT * FROM users WHERE age ! 18");
    expectError("extra word after value",     "SELECT * FROM users WHERE age > 18 extra");

    std::cout << "\nvalid AND --\n";
    expectWhere("one condition still works", "SELECT * FROM t WHERE a = 1",
                "t", "(a = 1)");
    expectWhere("two conditions",            "SELECT * FROM t WHERE a = 1 AND b = 2",
                "t", "((a = 1) AND (b = 2))");
    expectWhere("numbers and text",          "SELECT * FROM users WHERE age > 18 AND name = 'bob'",
                "users", "((age > 18) AND (name = 'bob'))");
    expectWhere("three conditions group left to right",
                "SELECT * FROM t WHERE a = 1 AND b = 2 AND c = 3",
                "t", "(((a = 1) AND (b = 2)) AND (c = 3))");
    expectWhere("lowercase and",             "select * from t where a = 1 and b = 2;",
                "t", "((a = 1) AND (b = 2))");

    {
        const std::string name = "root is AND with two COMPARE children";
        try {
            Statement s = parse(tokenize("SELECT * FROM t WHERE a = 1 AND b = 2"));
            if (!s.where)                                   fail(name, "where is nullptr");
            else if (s.where->kind != ExprKind::AND)        fail(name, "root should be AND");
            else if (!s.where->left || !s.where->right)     fail(name, "AND node needs a left and right child");
            else if (s.where->left->kind != ExprKind::COMPARE ||
                     s.where->right->kind != ExprKind::COMPARE) fail(name, "children should be COMPARE");
            else if (s.where->left->column != "a" || s.where->right->column != "b")
                                                            fail(name, "left should be a, right should be b");
            else                                            pass(name);
        } catch (const ParseError& e) {
            fail(name, std::string("should have worked, but got error: ") + e.what());
        }
    }

    std::cout << "\nbroken AND --\n";
    expectError("AND with nothing after",   "SELECT * FROM t WHERE a = 1 AND");
    expectError("AND at the start",         "SELECT * FROM t WHERE AND a = 1");
    expectError("AND twice in a row",       "SELECT * FROM t WHERE a = 1 AND AND b = 2");
    expectError("broken condition after AND","SELECT * FROM t WHERE a = 1 AND b 2");

    std::cout << "\nvalid OR --\n";
    expectWhere("two conditions with OR",   "SELECT * FROM t WHERE a = 1 OR b = 2",
                "t", "((a = 1) OR (b = 2))");
    expectWhere("three ORs group left to right", "SELECT * FROM t WHERE a = 1 OR b = 2 OR c = 3",
                "t", "(((a = 1) OR (b = 2)) OR (c = 3))");
    expectWhere("AND binds tighter (OR first)",  "SELECT * FROM t WHERE a = 1 OR b = 2 AND c = 3",
                "t", "((a = 1) OR ((b = 2) AND (c = 3)))");
    expectWhere("AND binds tighter (AND first)", "SELECT * FROM t WHERE a = 1 AND b = 2 OR c = 3",
                "t", "(((a = 1) AND (b = 2)) OR (c = 3))");
    expectWhere("two ANDs joined by OR",
                "SELECT * FROM t WHERE a = 1 AND b = 2 OR c = 3 AND d = 4",
                "t", "(((a = 1) AND (b = 2)) OR ((c = 3) AND (d = 4)))");
    expectWhere("AND alone still works",     "SELECT * FROM t WHERE a = 1 AND b = 2",
                "t", "((a = 1) AND (b = 2))");
    expectWhere("lowercase or",              "select * from t where a = 1 or b = 'x';",
                "t", "((a = 1) OR (b = 'x'))");

    {
        const std::string name = "root is OR, right child is AND";
        try {
            Statement s = parse(tokenize("SELECT * FROM t WHERE a = 1 OR b = 2 AND c = 3"));
            if (!s.where)                                  fail(name, "where is nullptr");
            else if (s.where->kind != ExprKind::OR)        fail(name, "root should be OR");
            else if (!s.where->left || !s.where->right)    fail(name, "OR node needs a left and right child");
            else if (s.where->left->kind != ExprKind::COMPARE) fail(name, "left child should be COMPARE (a = 1)");
            else if (s.where->right->kind != ExprKind::AND)    fail(name, "right child should be AND (b = 2 AND c = 3)");
            else                                           pass(name);
        } catch (const ParseError& e) {
            fail(name, std::string("should have worked, but got error: ") + e.what());
        }
    }

    std::cout << "\nbroken OR --\n";
    expectError("OR with nothing after",        "SELECT * FROM t WHERE a = 1 OR");
    expectError("OR at the start",              "SELECT * FROM t WHERE OR a = 1");
    expectError("OR twice in a row",            "SELECT * FROM t WHERE a = 1 OR OR b = 2");
    expectError("AND then OR with nothing between", "SELECT * FROM t WHERE a = 1 AND OR b = 2");

    std::cout << "\nvalid parentheses --\n";
    expectWhere("one condition in ()",       "SELECT * FROM t WHERE (a = 1)",
                "t", "(a = 1)");
    expectWhere("double () changes nothing", "SELECT * FROM t WHERE ((a = 1))",
                "t", "(a = 1)");
    expectWhere("() makes OR go first",      "SELECT * FROM t WHERE (a = 1 OR b = 2) AND c = 3",
                "t", "(((a = 1) OR (b = 2)) AND (c = 3))");
    expectWhere("() on the right side",      "SELECT * FROM t WHERE a = 1 AND (b = 2 OR c = 3)",
                "t", "((a = 1) AND ((b = 2) OR (c = 3)))");
    expectWhere("() around an AND",          "SELECT * FROM t WHERE (a = 1 AND b = 2) OR c = 3",
                "t", "(((a = 1) AND (b = 2)) OR (c = 3))");
    expectWhere("() inside ()",              "SELECT * FROM t WHERE ((a = 1 OR b = 2) AND c = 3) OR d = 4",
                "t", "((((a = 1) OR (b = 2)) AND (c = 3)) OR (d = 4))");
    expectWhere("no () still works the same","SELECT * FROM t WHERE a = 1 OR b = 2 AND c = 3",
                "t", "((a = 1) OR ((b = 2) AND (c = 3)))");
    expectWhere("() with semicolon",         "select * from t where (a = 1 or b = 2);",
                "t", "((a = 1) OR (b = 2))");

    {
        const std::string name = "root is AND, left child is the OR from ()";
        try {
            Statement s = parse(tokenize("SELECT * FROM t WHERE (a = 1 OR b = 2) AND c = 3"));
            if (!s.where)                                  fail(name, "where is nullptr");
            else if (s.where->kind != ExprKind::AND)       fail(name, "root should be AND");
            else if (!s.where->left || !s.where->right)    fail(name, "AND node needs a left and right child");
            else if (s.where->left->kind != ExprKind::OR)  fail(name, "left child should be OR (from the parentheses)");
            else if (s.where->right->kind != ExprKind::COMPARE) fail(name, "right child should be COMPARE (c = 3)");
            else                                           pass(name);
        } catch (const ParseError& e) {
            fail(name, std::string("should have worked, but got error: ") + e.what());
        }
    }

    std::cout << "\nbroken parentheses --\n";
    expectError("( never closed",            "SELECT * FROM t WHERE (a = 1");
    expectError(") with no (",               "SELECT * FROM t WHERE a = 1)");
    expectError("empty ()",                  "SELECT * FROM t WHERE ()");
    expectError("one ( too few",             "SELECT * FROM t WHERE ((a = 1)");
    expectError("one ) too many",            "SELECT * FROM t WHERE (a = 1))");
    expectError("backwards parentheses",     "SELECT * FROM t WHERE )a = 1(");
    expectError("AND right before )",        "SELECT * FROM t WHERE (a = 1 AND) OR b = 2");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}