// Executor tests: every statement, WHERE, ORDER BY, the error checks,
// hasUnsavedChanges and saveAll / loadAll.
// Run with:  make test-executor
//
// Every command goes through the real tokenizer and parser, then
// db.execute(). The tests check ok, affected, header and rows. They never
// check the exact wording of a message, so word them however you like.
//
// Sections 10 (part of it) and 11 also need storage.cpp (saveTable /
// loadTable) to be finished. Files go to executor_test_tmp/ and it gets
// deleted at the end.

#include "../src/executor.h"
#include "../src/parser.h"
#include "../src/tokenizer.h"
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static void onCrash(int) {
    const char msg[] =
        "\n  CRASH  the test right after the last PASS/FAIL line above crashed\n"
        "         (usually a null Table*, row[i] with a bad i, or changing the\n"
        "         tree while walking through it).\n"
        "         Run `make test-asan T=test-executor` to see the exact line.\n";
    ssize_t ignored = write(1, msg, sizeof(msg) - 1);
    (void)ignored;
    _exit(1);
}

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

// ---------- helpers ----------

static const std::string DIR = "executor_test_tmp";

// Rows in the tests are written as plain text: {"1", "alice", "30"}
using TextRow = std::vector<std::string>;

static Value I(long long n) { return Value{ValueType::INT, std::to_string(n)}; }
static Value T(const std::string& s) { return Value{ValueType::TEXT, s}; }

// Parses and runs one command. A parse error comes back as ok = false
// (and the message says so, so a typo in a test is easy to spot).
static Result run(Database& db, const std::string& sql) {
    try {
        return db.execute(parse(tokenize(sql)));
    } catch (const ParseError& e) {
        Result r;
        r.ok = false;
        r.message = std::string("PARSE ERROR IN THE TEST: ") + e.what();
        return r;
    }
}

static std::string join(const std::vector<std::string>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); i++) out += (i ? ", " : "") + v[i];
    return out + "]";
}

static TextRow toText(const Row& r) {
    TextRow out;
    for (const Value& v : r) out.push_back(v.text);
    return out;
}

static std::string rowsToString(const std::vector<TextRow>& rows) {
    if (rows.empty()) return "(no rows)";
    std::string out;
    for (const TextRow& r : rows) out += join(r) + " ";
    return out;
}

static std::vector<TextRow> toText(const std::vector<Row>& rows) {
    std::vector<TextRow> out;
    for (const Row& r : rows) out.push_back(toText(r));
    return out;
}

// What a statement said when it should have worked.
static std::string whyFailed(const Result& r) {
    return "ok was false, message: \"" + r.message + "\"";
}

// Every row in the table, in key order, as text.
static std::vector<TextRow> tableRows(const Database& db, const std::string& name) {
    std::vector<TextRow> out;
    const Table* t = db.getTable(name);
    if (!t) return out;
    for (const auto& kv : t->rows.traverseInOrder()) out.push_back(toText(kv.second));
    return out;
}

// The statement must work, change `affected` rows, and leave the table
// holding exactly `expected` (in key order) in a valid tree.
static void checkChange(const std::string& name, Database& db, const std::string& sql,
                        size_t affected, const std::string& table,
                        const std::vector<TextRow>& expected) {
    Result r = run(db, sql);
    if (!r.ok) { fail(name, sql + "\n        " + whyFailed(r)); return; }
    if (r.affected != affected) {
        fail(name, sql + "\n        affected should be " + std::to_string(affected) +
                   " but was " + std::to_string(r.affected));
        return;
    }
    std::vector<TextRow> got = tableRows(db, table);
    if (got != expected) {
        fail(name, sql + "\n        table should hold " + rowsToString(expected) +
                   "\n        but holds " + rowsToString(got));
        return;
    }
    std::string why;
    const Table* t = db.getTable(table);
    if (t && !t->rows.checkValid(&why)) { fail(name, "tree is broken after " + sql + ": " + why); return; }
    pass(name);
}

// A SELECT must work and give exactly this header and these rows, in this order.
static void checkSelect(const std::string& name, Database& db, const std::string& sql,
                        const std::vector<std::string>& header,
                        const std::vector<TextRow>& rows) {
    Result r = run(db, sql);
    if (!r.ok) { fail(name, sql + "\n        " + whyFailed(r)); return; }
    if (r.header != header) {
        fail(name, sql + "\n        header should be " + join(header) + " but was " + join(r.header));
        return;
    }
    std::vector<TextRow> got = toText(r.rows);
    if (got != rows) {
        fail(name, sql + "\n        rows should be " + rowsToString(rows) + "\n        but were " + rowsToString(got));
        return;
    }
    if (r.affected != rows.size()) {
        fail(name, sql + "\n        affected should be the number of rows (" + std::to_string(rows.size()) +
                   ") but was " + std::to_string(r.affected));
        return;
    }
    pass(name);
}

// Builds a whole picture of the database (names, columns, rows) so we can
// check that a failed statement changed NOTHING.
static std::string snapshot(const Database& db) {
    std::string out;
    for (const std::string& name : db.tableNames()) {
        const Table* t = db.getTable(name);
        out += name + "(";
        for (const Column& c : t->columns) out += c.name + (c.type == ValueType::INT ? " INT," : " TEXT,");
        out += ") " + rowsToString(tableRows(db, name)) + "\n";
    }
    return out;
}

// The statement must fail (ok = false) and the database must be exactly as before.
static void checkError(const std::string& name, Database& db, const std::string& sql) {
    std::string before = snapshot(db);
    Result r = run(db, sql);
    std::string after = snapshot(db);
    if (r.ok) fail(name, sql + "\n        should fail, but ok was true");
    else if (r.message.rfind("PARSE ERROR", 0) == 0) fail(name, r.message);
    else if (before != after) fail(name, sql + "\n        failed but changed the database. Before:\n" + before + "        After:\n" + after);
    else if (r.message.empty()) fail(name, sql + "\n        failed with an empty message (say what went wrong)");
    else pass(name);
}

// Runs setup commands. Returns false (and FAILs the test) if any doesn't work,
// so a later test never passes just because nothing ran.
static bool setup(const std::string& name, Database& db, const std::vector<std::string>& sqls) {
    for (const std::string& sql : sqls) {
        Result r = run(db, sql);
        if (!r.ok) {
            fail(name, "setup failed on " + sql + "\n        " + whyFailed(r));
            return false;
        }
    }
    return true;
}

// The users table most tests use:
//   id | name  | age
//    1 | alice | 30
//    2 | bob   | 25
//    3 | carol | 35
//    4 | dave  | 25
//    5 | eve   | 40
static const std::vector<std::string> USERS = {
    "CREATE TABLE users (id INT, name TEXT, age INT)",
    "INSERT INTO users VALUES (3, 'carol', 35)",
    "INSERT INTO users VALUES (1, 'alice', 30)",
    "INSERT INTO users VALUES (5, 'eve', 40)",
    "INSERT INTO users VALUES (2, 'bob', 25)",
    "INSERT INTO users VALUES (4, 'dave', 25)",
};
static const std::vector<TextRow> ALL_USERS = {
    {"1", "alice", "30"}, {"2", "bob", "25"}, {"3", "carol", "35"}, {"4", "dave", "25"}, {"5", "eve", "40"}};
static const std::vector<std::string> USERS_HEADER = {"id", "name", "age"};

// Ids 2, 9, 10, 100 and -7: compared as text, "10" < "9", so these catch
// INT values being compared alphabetically.
static const std::vector<std::string> NUMS = {
    "CREATE TABLE nums (id INT, label TEXT)",
    "INSERT INTO nums VALUES (10, 'ten')",
    "INSERT INTO nums VALUES (2, 'two')",
    "INSERT INTO nums VALUES (100, 'hundred')",
    "INSERT INTO nums VALUES (-7, 'minus seven')",
    "INSERT INTO nums VALUES (9, 'nine')",
};

// Fills db with the users and nums tables. false (and a FAIL) if that didn't work.
static bool makeUsers(const std::string& name, Database& db) {
    return setup(name, db, USERS) && setup(name, db, NUMS);
}

// WHERE test: SELECT * FROM users WHERE <cond> must give these ids, in key order.
static void checkWhere(const std::string& cond, const std::vector<std::string>& ids) {
    std::string name = "WHERE " + cond;
    Database db;
    if (!makeUsers(name, db)) return;
    std::vector<TextRow> expected;
    for (const TextRow& r : ALL_USERS)
        for (const std::string& id : ids)
            if (r[0] == id) expected.push_back(r);
    checkSelect(name, db, "SELECT * FROM users WHERE " + cond, USERS_HEADER, expected);
}

static void checkNums(const std::string& cond, const std::vector<std::string>& ids) {
    std::string name = "nums WHERE " + cond;
    Database db;
    if (!makeUsers(name, db)) return;
    Result r = run(db, "SELECT id FROM nums WHERE " + cond);
    std::vector<std::string> got;
    for (const Row& row : r.rows) got.push_back(row.empty() ? "?" : row[0].text);
    if (!r.ok) fail(name, whyFailed(r));
    else if (got != ids) fail(name, "ids should be " + join(ids) + " but were " + join(got));
    else pass(name);
}

static void writeFile(const std::string& p, const std::string& content) {
    std::ofstream out(p, std::ios::binary);
    out << content;
}

static std::string readFile(const std::string& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    std::cout << std::unitbuf;
    std::signal(SIGSEGV, onCrash);
    std::signal(SIGABRT, onCrash);

    fs::remove_all(DIR);
    fs::create_directory(DIR);

    // ------------------------------------------------------------------
    std::cout << "\n-- 1. CREATE --\n";
    {
        std::string name = "CREATE works and affects 0 rows";
        Database db;
        Result r = run(db, "CREATE TABLE users (id INT, name TEXT, age INT)");
        if (!r.ok) fail(name, whyFailed(r));
        else if (r.affected != 0) fail(name, "affected should be 0 but was " + std::to_string(r.affected));
        else pass(name);
    }
    {
        std::string name = "the table exists with the right columns";
        Database db;
        run(db, "CREATE TABLE users (id INT, name TEXT, age INT)");
        const Table* t = db.getTable("users");
        if (!db.hasTable("users") || !t) fail(name, "hasTable/getTable can't find users");
        else if (t->columns.size() != 3 ||
                 t->columns[0].name != "id"   || t->columns[0].type != ValueType::INT ||
                 t->columns[1].name != "name" || t->columns[1].type != ValueType::TEXT ||
                 t->columns[2].name != "age"  || t->columns[2].type != ValueType::INT)
            fail(name, "columns should be id INT, name TEXT, age INT");
        else pass(name);
    }
    {
        std::string name = "a new table is empty";
        Database db;
        run(db, "CREATE TABLE users (id INT, name TEXT, age INT)");
        const Table* t = db.getTable("users");
        if (!t) fail(name, "table wasn't created");
        else if (t->rows.size() != 0) fail(name, "has " + std::to_string(t->rows.size()) + " rows");
        else pass(name);
    }
    {
        std::string name = "tableNames is sorted";
        Database db;
        if (setup(name, db, {"CREATE TABLE zeta (a INT)", "CREATE TABLE alpha (a INT)", "CREATE TABLE mid (a TEXT)"})) {
            std::vector<std::string> expected = {"alpha", "mid", "zeta"};
            if (db.tableNames() != expected) fail(name, "got " + join(db.tableNames()));
            else pass(name);
        }
    }
    {
        std::string name = "other tables aren't there";
        Database db;
        if (setup(name, db, {"CREATE TABLE users (id INT)"})) {
            if (db.hasTable("orders") || db.getTable("orders") != nullptr) fail(name, "found a table called orders");
            else pass(name);
        }
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 2. INSERT --\n";
    {
        Database db;
        if (setup("INSERT works and affects 1 row", db, {"CREATE TABLE users (id INT, name TEXT, age INT)"}))
            checkChange("INSERT works and affects 1 row", db, "INSERT INTO users VALUES (1, 'alice', 30)",
                        1, "users", {{"1", "alice", "30"}});
    }
    {
        std::string name = "the row is in the tree under its key, with the right types";
        Database db;
        if (setup(name, db, {"CREATE TABLE users (id INT, name TEXT, age INT)", "INSERT INTO users VALUES (7, 'bob', 25)"})) {
            const Row* r = db.getTable("users")->rows.find(I(7));
            if (!r) fail(name, "rows.find(7) gave nullptr");
            else if (r->size() != 3 || (*r)[0].type != ValueType::INT || (*r)[1].type != ValueType::TEXT ||
                     (*r)[2].type != ValueType::INT || toText(*r) != TextRow{"7", "bob", "25"})
                fail(name, "row is " + join(toText(*r)));
            else pass(name);
        }
    }
    {
        std::string name = "rows inserted out of order come back in key order";
        Database db;
        if (setup(name, db, USERS)) {
            if (tableRows(db, "users") != ALL_USERS) fail(name, "table holds " + rowsToString(tableRows(db, "users")));
            else pass(name);
        }
    }
    {
        std::string name = "200 rows, tree stays valid";
        Database db;
        if (setup(name, db, {"CREATE TABLE big (id INT, v TEXT)"})) {
            bool ok = true;
            for (int k = 200; k >= 1 && ok; k--) {
                Result r = run(db, "INSERT INTO big VALUES (" + std::to_string((k * 37) % 200 + 1) + ", 'x')");
                if (!r.ok) { fail(name, "insert number " + std::to_string(201 - k) + ": " + whyFailed(r)); ok = false; }
            }
            std::string why;
            if (ok) {
                const Table* t = db.getTable("big");
                if (t->rows.size() != 200) fail(name, "size is " + std::to_string(t->rows.size()));
                else if (!t->rows.checkValid(&why)) fail(name, why);
                else pass(name);
            }
        }
    }
    {
        Database db;
        if (setup("TEXT key column", db, {"CREATE TABLE pets (name TEXT, age INT)", "INSERT INTO pets VALUES ('rex', 3)"}))
            checkChange("TEXT key column", db, "INSERT INTO pets VALUES ('fido', 5)", 1, "pets",
                        {{"fido", "5"}, {"rex", "3"}});
    }
    {
        Database db;
        if (setup("negative numbers", db, {"CREATE TABLE t (id INT, x INT)"}))
            checkChange("negative numbers", db, "INSERT INTO t VALUES (-3, -100)", 1, "t", {{"-3", "-100"}});
    }
    {
        Database db;
        if (setup("text with spaces, commas and quotes", db, {"CREATE TABLE t (id INT, s TEXT)"}))
            checkChange("text with spaces, commas and quotes", db, "INSERT INTO t VALUES (1, 'hi, \"bob\"  ')",
                        1, "t", {{"1", "hi, \"bob\"  "}});
    }
    {
        std::string name = "INSERT only touches its own table";
        Database db;
        if (setup(name, db, {"CREATE TABLE a (id INT)", "CREATE TABLE b (id INT)", "INSERT INTO a VALUES (1)"})) {
            if (db.getTable("b")->rows.size() != 0) fail(name, "table b got a row");
            else if (db.getTable("a")->rows.size() != 1) fail(name, "table a should have 1 row");
            else pass(name);
        }
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 3. SELECT * and columns --\n";
    {
        Database db;
        if (setup("SELECT * gives every row in key order", db, USERS))
            checkSelect("SELECT * gives every row in key order", db, "SELECT * FROM users", USERS_HEADER, ALL_USERS);
    }
    {
        Database db;
        if (setup("SELECT * from an empty table", db, {"CREATE TABLE users (id INT, name TEXT, age INT)"}))
            checkSelect("SELECT * from an empty table", db, "SELECT * FROM users", USERS_HEADER, {});
    }
    {
        Database db;
        if (setup("SELECT some columns", db, USERS))
            checkSelect("SELECT some columns", db, "SELECT name, age FROM users", {"name", "age"},
                        {{"alice", "30"}, {"bob", "25"}, {"carol", "35"}, {"dave", "25"}, {"eve", "40"}});
    }
    {
        Database db;
        if (setup("SELECT columns in a different order", db, USERS))
            checkSelect("SELECT columns in a different order", db, "SELECT age, id FROM users", {"age", "id"},
                        {{"30", "1"}, {"25", "2"}, {"35", "3"}, {"25", "4"}, {"40", "5"}});
    }
    {
        Database db;
        if (setup("SELECT one column", db, USERS))
            checkSelect("SELECT one column", db, "SELECT name FROM users", {"name"},
                        {{"alice"}, {"bob"}, {"carol"}, {"dave"}, {"eve"}});
    }
    {
        std::string name = "SELECT keeps the value types";
        Database db;
        if (setup(name, db, USERS)) {
            Result r = run(db, "SELECT age, name FROM users");
            if (!r.ok) fail(name, whyFailed(r));
            else if (r.rows.empty() || r.rows[0].size() != 2) fail(name, "expected rows of 2 values");
            else if (r.rows[0][0].type != ValueType::INT || r.rows[0][1].type != ValueType::TEXT)
                fail(name, "age should be INT and name TEXT");
            else pass(name);
        }
    }
    {
        std::string name = "SELECT doesn't change the table";
        Database db;
        if (setup(name, db, USERS)) {
            run(db, "SELECT name FROM users WHERE age > 30");
            if (tableRows(db, "users") != ALL_USERS) fail(name, "table now holds " + rowsToString(tableRows(db, "users")));
            else pass(name);
        }
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 4. WHERE --\n";
    {
        std::string name = "matches(nullptr, ...) is true";
        std::vector<Column> cols = {Column{ValueType::INT, "id"}};
        if (!matches(nullptr, Row{I(1)}, cols)) fail(name, "returned false");
        else pass(name);
    }
    {
        std::string name = "matches() on a hand-built COMPARE";
        std::vector<Column> cols = {Column{ValueType::INT, "id"}, Column{ValueType::TEXT, "name"}};
        Expr e;
        e.kind = ExprKind::COMPARE;
        e.column = "name";
        e.op = TokenType::EQUAL;
        e.value = T("bob");
        bool yes = matches(&e, Row{I(1), T("bob")}, cols);
        bool no = matches(&e, Row{I(2), T("amy")}, cols);
        if (!yes) fail(name, "name = 'bob' should match {1, bob}");
        else if (no) fail(name, "name = 'bob' should not match {2, amy}");
        else pass(name);
    }
    {
        std::string name = "matches() on a column that doesn't exist is false";
        std::vector<Column> cols = {Column{ValueType::INT, "id"}};
        Expr e;
        e.column = "nope";
        e.op = TokenType::EQUAL;
        e.value = I(1);
        if (matches(&e, Row{I(1)}, cols)) fail(name, "returned true");
        else pass(name);
    }
    checkWhere("id = 3", {"3"});
    checkWhere("id != 3", {"1", "2", "4", "5"});
    checkWhere("age < 30", {"2", "4"});
    checkWhere("age > 30", {"3", "5"});
    checkWhere("age <= 30", {"1", "2", "4"});
    checkWhere("age >= 35", {"3", "5"});
    checkWhere("id = 99", {});
    checkNums("id > 9", {"10", "100"});
    checkNums("id < 10", {"-7", "2", "9"});
    checkNums("id >= -7", {"-7", "2", "9", "10", "100"});
    checkNums("id < -1", {"-7"});
    checkWhere("name = 'bob'", {"2"});
    checkWhere("name != 'bob'", {"1", "3", "4", "5"});
    checkWhere("name < 'c'", {"1", "2"});
    checkWhere("name > 'carol'", {"4", "5"});
    checkWhere("name >= 'carol'", {"3", "4", "5"});
    checkWhere("name <= 'b'", {"1"});
    checkWhere("name = 'Bob'", {});
    checkWhere("age = 25 AND name = 'dave'", {"4"});
    checkWhere("age > 25 AND age < 40", {"1", "3"});
    checkWhere("id = 1 OR id = 5", {"1", "5"});
    checkWhere("name = 'eve' OR age = 25", {"2", "4", "5"});
    checkWhere("id = 1 OR age = 25 AND name = 'dave'", {"1", "4"});
    checkWhere("(id = 1 OR age = 25) AND name != 'bob'", {"1", "4"});
    checkWhere("id = 1 OR (age = 25 AND name = 'dave')", {"1", "4"});
    checkWhere("(age < 30 OR age > 35) AND (name < 'c' OR id = 5)", {"2", "5"});
    {
        Database db;
        if (makeUsers("WHERE on a column you didn't SELECT", db))
            checkSelect("WHERE on a column you didn't SELECT", db, "SELECT name FROM users WHERE age > 30",
                        {"name"}, {{"carol"}, {"eve"}});
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 5. UPDATE --\n";
    {
        Database db;
        if (setup("UPDATE one row", db, USERS))
            checkChange("UPDATE one row", db, "UPDATE users SET age = 99 WHERE id = 2", 1, "users",
                        {{"1", "alice", "30"}, {"2", "bob", "99"}, {"3", "carol", "35"}, {"4", "dave", "25"}, {"5", "eve", "40"}});
    }
    {
        Database db;
        if (setup("UPDATE two columns at once", db, USERS))
            checkChange("UPDATE two columns at once", db, "UPDATE users SET name = 'zed', age = 1 WHERE id = 5", 1, "users",
                        {{"1", "alice", "30"}, {"2", "bob", "25"}, {"3", "carol", "35"}, {"4", "dave", "25"}, {"5", "zed", "1"}});
    }
    {
        Database db;
        if (setup("UPDATE several rows", db, USERS))
            checkChange("UPDATE several rows", db, "UPDATE users SET name = 'twentyfive' WHERE age = 25", 2, "users",
                        {{"1", "alice", "30"}, {"2", "twentyfive", "25"}, {"3", "carol", "35"}, {"4", "twentyfive", "25"}, {"5", "eve", "40"}});
    }
    {
        Database db;
        if (setup("UPDATE the column used in WHERE", db, USERS))
            checkChange("UPDATE the column used in WHERE", db, "UPDATE users SET age = 26 WHERE age = 25", 2, "users",
                        {{"1", "alice", "30"}, {"2", "bob", "26"}, {"3", "carol", "35"}, {"4", "dave", "26"}, {"5", "eve", "40"}});
    }
    {
        Database db;
        if (setup("UPDATE with no WHERE changes every row", db, USERS))
            checkChange("UPDATE with no WHERE changes every row", db, "UPDATE users SET age = 0", 5, "users",
                        {{"1", "alice", "0"}, {"2", "bob", "0"}, {"3", "carol", "0"}, {"4", "dave", "0"}, {"5", "eve", "0"}});
    }
    {
        Database db;
        if (setup("UPDATE that matches nothing", db, USERS))
            checkChange("UPDATE that matches nothing", db, "UPDATE users SET age = 1 WHERE age > 100", 0, "users", ALL_USERS);
    }
    {
        std::string name = "UPDATE on 100 rows";
        Database db;
        std::vector<std::string> sqls = {"CREATE TABLE t (id INT, v INT)"};
        for (int k = 1; k <= 100; k++) sqls.push_back("INSERT INTO t VALUES (" + std::to_string(k) + ", 0)");
        if (setup(name, db, sqls)) {
            std::vector<TextRow> expected;
            for (int k = 1; k <= 100; k++) expected.push_back({std::to_string(k), k > 50 ? "1" : "0"});
            checkChange(name, db, "UPDATE t SET v = 1 WHERE id > 50", 50, "t", expected);
        }
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 6. DELETE --\n";
    {
        Database db;
        if (setup("DELETE one row", db, USERS))
            checkChange("DELETE one row", db, "DELETE FROM users WHERE id = 3", 1, "users",
                        {{"1", "alice", "30"}, {"2", "bob", "25"}, {"4", "dave", "25"}, {"5", "eve", "40"}});
    }
    {
        Database db;
        if (setup("DELETE several rows", db, USERS))
            checkChange("DELETE several rows", db, "DELETE FROM users WHERE age = 25 OR name = 'eve'", 3, "users",
                        {{"1", "alice", "30"}, {"3", "carol", "35"}});
    }
    {
        std::string name = "DELETE with no WHERE empties the table";
        Database db;
        if (setup(name, db, USERS)) {
            checkChange(name, db, "DELETE FROM users", 5, "users", {});
            if (!db.hasTable("users")) fail("DELETE with no WHERE keeps the table", "the table is gone");
            else pass("DELETE with no WHERE keeps the table");
        }
    }
    {
        Database db;
        if (setup("DELETE that matches nothing", db, USERS))
            checkChange("DELETE that matches nothing", db, "DELETE FROM users WHERE age > 100", 0, "users", ALL_USERS);
    }
    {
        std::string name = "DELETE 150 rows out of 300";
        Database db;
        std::vector<std::string> sqls = {"CREATE TABLE t (id INT, v TEXT)"};
        for (int k = 1; k <= 300; k++) sqls.push_back("INSERT INTO t VALUES (" + std::to_string(k) + ", 'v')");
        if (setup(name, db, sqls)) {
            std::vector<TextRow> expected;
            for (int k = 1; k <= 300; k++)
                if (k <= 100 || k > 250) expected.push_back({std::to_string(k), "v"});
            checkChange(name, db, "DELETE FROM t WHERE id > 100 AND id <= 250", 150, "t", expected);
        }
    }
    {
        Database db;
        if (setup("DELETE then INSERT the same key again", db, USERS) &&
            setup("DELETE then INSERT the same key again", db, {"DELETE FROM users WHERE id = 2"}))
            checkChange("DELETE then INSERT the same key again", db, "INSERT INTO users VALUES (2, 'new bob', 1)", 1, "users",
                        {{"1", "alice", "30"}, {"2", "new bob", "1"}, {"3", "carol", "35"}, {"4", "dave", "25"}, {"5", "eve", "40"}});
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 7. DROP / TRUNCATE --\n";
    {
        std::string name = "DROP removes the table";
        Database db;
        if (makeUsers(name, db)) {
            Result r = run(db, "DROP TABLE users");
            if (!r.ok) fail(name, whyFailed(r));
            else if (db.hasTable("users") || db.getTable("users")) fail(name, "users is still there");
            else if (db.tableNames() != std::vector<std::string>{"nums"}) fail(name, "tableNames is " + join(db.tableNames()));
            else if (tableRows(db, "nums").size() != 5) fail(name, "nums was changed");
            else pass(name);
        }
    }
    {
        std::string name = "CREATE again after DROP gives an empty table";
        Database db;
        if (setup(name, db, USERS) && setup(name, db, {"DROP TABLE users", "CREATE TABLE users (k TEXT)"})) {
            const Table* t = db.getTable("users");
            if (t->rows.size() != 0) fail(name, "has " + std::to_string(t->rows.size()) + " rows");
            else if (t->columns.size() != 1 || t->columns[0].name != "k") fail(name, "has the old columns");
            else pass(name);
        }
    }
    {
        Database db;
        if (setup("TRUNCATE removes every row", db, USERS))
            checkChange("TRUNCATE removes every row", db, "TRUNCATE TABLE users", 5, "users", {});
    }
    {
        std::string name = "TRUNCATE keeps the table and its columns";
        Database db;
        if (setup(name, db, USERS) && setup(name, db, {"TRUNCATE TABLE users"})) {
            const Table* t = db.getTable("users");
            if (!t) fail(name, "the table is gone");
            else if (t->columns.size() != 3 || t->columns[1].name != "name") fail(name, "columns changed");
            else pass(name);
        }
    }
    {
        Database db;
        if (setup("INSERT after TRUNCATE", db, USERS) && setup("INSERT after TRUNCATE", db, {"TRUNCATE TABLE users"}))
            checkChange("INSERT after TRUNCATE", db, "INSERT INTO users VALUES (1, 'again', 2)", 1, "users",
                        {{"1", "again", "2"}});
    }
    {
        std::string name = "TRUNCATE only touches its own table";
        Database db;
        if (makeUsers(name, db) && setup(name, db, {"TRUNCATE TABLE nums"})) {
            if (tableRows(db, "users") != ALL_USERS) fail(name, "users was changed");
            else pass(name);
        }
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 8. ORDER BY --\n";
    {
        Database db;
        if (setup("ORDER BY an INT column", db, USERS))
            checkSelect("ORDER BY an INT column", db, "SELECT * FROM users WHERE age != 25 ORDER BY age", USERS_HEADER,
                        {{"1", "alice", "30"}, {"3", "carol", "35"}, {"5", "eve", "40"}});
    }
    {
        Database db;
        if (setup("ORDER BY ... ASC", db, USERS))
            checkSelect("ORDER BY ... ASC", db, "SELECT name FROM users ORDER BY name ASC", {"name"},
                        {{"alice"}, {"bob"}, {"carol"}, {"dave"}, {"eve"}});
    }
    {
        Database db;
        if (setup("ORDER BY ... DESC", db, USERS))
            checkSelect("ORDER BY ... DESC", db, "SELECT name FROM users ORDER BY name DESC", {"name"},
                        {{"eve"}, {"dave"}, {"carol"}, {"bob"}, {"alice"}});
    }
    {
        Database db;
        if (makeUsers("ORDER BY sorts INT as numbers", db))
            checkSelect("ORDER BY sorts INT as numbers", db, "SELECT id FROM nums ORDER BY id DESC", {"id"},
                        {{"100"}, {"10"}, {"9"}, {"2"}, {"-7"}});
    }
    {
        Database db;
        if (makeUsers("ORDER BY a column you didn't SELECT", db))
            checkSelect("ORDER BY a column you didn't SELECT", db, "SELECT id FROM nums ORDER BY label", {"id"},
                        {{"100"}, {"-7"}, {"9"}, {"10"}, {"2"}});
    }
    {
        Database db;
        if (setup("ORDER BY two columns", db, USERS))
            checkSelect("ORDER BY two columns", db, "SELECT name, age FROM users ORDER BY age, name DESC", {"name", "age"},
                        {{"dave", "25"}, {"bob", "25"}, {"alice", "30"}, {"carol", "35"}, {"eve", "40"}});
    }
    {
        Database db;
        if (setup("ORDER BY two columns, both DESC", db, USERS))
            checkSelect("ORDER BY two columns, both DESC", db, "SELECT id FROM users ORDER BY age DESC, id DESC", {"id"},
                        {{"5"}, {"3"}, {"1"}, {"4"}, {"2"}});
    }
    {
        Database db;
        if (setup("ORDER BY with WHERE", db, USERS))
            checkSelect("ORDER BY with WHERE", db, "SELECT name FROM users WHERE age < 40 ORDER BY age DESC, id", {"name"},
                        {{"carol"}, {"alice"}, {"bob"}, {"dave"}});
    }
    {
        Database db;
        if (setup("ORDER BY doesn't change the table", db, USERS) &&
            setup("ORDER BY doesn't change the table", db, {"SELECT * FROM users ORDER BY name DESC"})) {
            if (tableRows(db, "users") != ALL_USERS) fail("ORDER BY doesn't change the table", "table order changed");
            else pass("ORDER BY doesn't change the table");
        }
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 9. errors (must fail AND change nothing) --\n";
    {
        Database db;
        if (makeUsers("errors: setup", db)) {
            pass("errors: setup");

            checkError("CREATE a table that already exists", db, "CREATE TABLE users (x TEXT)");
            checkError("CREATE with two columns of the same name", db, "CREATE TABLE t (a INT, b TEXT, a TEXT)");
            if (db.hasTable("t")) fail("failed CREATE doesn't make the table", "table t exists");
            else pass("failed CREATE doesn't make the table");

            checkError("INSERT into a table that doesn't exist", db, "INSERT INTO nope VALUES (1)");
            if (db.hasTable("nope")) fail("failed INSERT doesn't make a table", "table nope exists");
            else pass("failed INSERT doesn't make a table");
            checkError("INSERT too few values", db, "INSERT INTO users VALUES (9, 'x')");
            checkError("INSERT too many values", db, "INSERT INTO users VALUES (9, 'x', 1, 2)");
            checkError("INSERT text into an INT column", db, "INSERT INTO users VALUES (9, 'x', 'old')");
            checkError("INSERT text into the INT key", db, "INSERT INTO users VALUES ('9', 'x', 1)");
            checkError("INSERT a number into a TEXT column", db, "INSERT INTO users VALUES (9, 42, 1)");
            checkError("INSERT a duplicate key", db, "INSERT INTO users VALUES (3, 'impostor', 1)");
            checkError("INSERT a duplicate key written differently (03)", db, "INSERT INTO users VALUES (03, 'impostor', 1)");
            checkError("INSERT a number too big for a long long", db, "INSERT INTO users VALUES (99999999999999999999, 'x', 1)");

            checkError("SELECT from a table that doesn't exist", db, "SELECT * FROM nope");
            checkError("SELECT a column that doesn't exist", db, "SELECT name, nope FROM users");
            checkError("SELECT with WHERE on a column that doesn't exist", db, "SELECT * FROM users WHERE nope = 1");
            checkError("SELECT ORDER BY a column that doesn't exist", db, "SELECT * FROM users ORDER BY nope");
            checkError("SELECT comparing an INT column with text", db, "SELECT * FROM users WHERE age = 'old'");
            checkError("SELECT comparing a TEXT column with a number", db, "SELECT * FROM users WHERE name > 5");
            checkError("SELECT with a bad column inside AND", db, "SELECT * FROM users WHERE id = 1 AND nope = 2");

            checkError("UPDATE a table that doesn't exist", db, "UPDATE nope SET a = 1");
            checkError("UPDATE SET a column that doesn't exist", db, "UPDATE users SET nope = 1 WHERE id = 1");
            checkError("UPDATE with WHERE on a column that doesn't exist", db, "UPDATE users SET age = 1 WHERE nope = 1");
            checkError("UPDATE comparing an INT column with text", db, "UPDATE users SET age = 1 WHERE id = 'one'");
            checkError("UPDATE comparing a TEXT column with a number", db, "UPDATE users SET age = 1 WHERE name = 1");
            checkError("UPDATE the key column", db, "UPDATE users SET id = 50 WHERE id = 1");
            checkError("UPDATE the key column of every row", db, "UPDATE users SET id = 7");
            checkError("UPDATE SET text into an INT column", db, "UPDATE users SET age = 'old' WHERE id = 1");
            checkError("UPDATE one good SET and one bad SET changes nothing", db, "UPDATE users SET age = 1, nope = 2");

            checkError("DELETE from a table that doesn't exist", db, "DELETE FROM nope");
            checkError("DELETE with WHERE on a column that doesn't exist", db, "DELETE FROM users WHERE nope = 1");
            checkError("UPDATE a key that doesn't exist", db, "UPDATE users SET age = 1 WHERE id = 99");
            checkError("UPDATE a missing key with more conditions", db, "UPDATE users SET age = 1 WHERE id = 99 AND age > 0");
            checkError("DELETE a key that doesn't exist", db, "DELETE FROM users WHERE id = 99");
            checkError("DELETE comparing an INT column with text", db, "DELETE FROM users WHERE age < 'x'");
            checkError("DELETE comparing a TEXT column with a number", db, "DELETE FROM users WHERE name = 1");
            checkError("DELETE with a bad column on the right of OR", db, "DELETE FROM users WHERE id = 1 OR nope = 2");
            checkError("DELETE with a bad compare deep inside ()", db,
                       "DELETE FROM users WHERE id = 1 OR (age > 1 AND (name = 'a' OR age = 'b'))");

            checkError("DROP a table that doesn't exist", db, "DROP TABLE nope");
            checkError("TRUNCATE a table that doesn't exist", db, "TRUNCATE TABLE nope");

            if (tableRows(db, "users") != ALL_USERS) fail("users is exactly as it started", "holds " + rowsToString(tableRows(db, "users")));
            else pass("users is exactly as it started");
        }
    }
    {
        std::string name = "a TEXT key table rejects a number key";
        Database db;
        if (setup(name, db, {"CREATE TABLE pets (name TEXT, age INT)"}))
            checkError(name, db, "INSERT INTO pets VALUES (5, 5)");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 10. hasUnsavedChanges --\n";
    {
        Database db;
        if (db.hasUnsavedChanges()) fail("a new database has no unsaved changes", "returned true");
        else pass("a new database has no unsaved changes");
    }
    {
        Database db;
        run(db, "DROP TABLE nope");
        run(db, "SELECT * FROM nope");
        if (db.hasUnsavedChanges()) fail("failed statements aren't changes", "returned true");
        else pass("failed statements aren't changes");
    }
    {
        std::string name = "CREATE is a change";
        Database db;
        if (setup(name, db, {"CREATE TABLE t (id INT)"})) {
            if (!db.hasUnsavedChanges()) fail(name, "returned false");
            else pass(name);
        }
    }
    std::cout << "   (the rest need storage.cpp)\n";
    {
        std::string name = "saveAll clears it";
        Database db;
        if (setup(name, db, USERS)) {
            Result r = db.saveAll(DIR + "/unsaved");
            if (!r.ok) fail(name, "saveAll failed: " + r.message);
            else if (db.hasUnsavedChanges()) fail(name, "still true after saveAll");
            else pass(name);
        }
    }
    {
        std::string name = "SELECT and failed statements after a save aren't changes";
        Database db;
        if (setup(name, db, USERS)) {
            Result r = db.saveAll(DIR + "/unsaved");
            run(db, "SELECT * FROM users WHERE age > 1 ORDER BY name");
            run(db, "INSERT INTO users VALUES (1, 'dup', 1)");
            run(db, "UPDATE users SET id = 9");
            if (!r.ok) fail(name, "saveAll failed: " + r.message);
            else if (db.hasUnsavedChanges()) fail(name, "returned true");
            else pass(name);
        }
    }
    for (const std::string& sql : {std::string("INSERT INTO users VALUES (9, 'x', 1)"),
                                   std::string("UPDATE users SET age = 1 WHERE id = 1"),
                                   std::string("DELETE FROM users WHERE id = 1"),
                                   std::string("CREATE TABLE other (id INT)"),
                                   std::string("DROP TABLE users"),
                                   std::string("TRUNCATE TABLE users")}) {
        std::string name = "after a save, " + sql.substr(0, sql.find(' ')) + " is a change";
        Database db;
        if (setup(name, db, USERS)) {
            Result saved = db.saveAll(DIR + "/unsaved");
            Result r = run(db, sql);
            if (!saved.ok) fail(name, "saveAll failed: " + saved.message);
            else if (!r.ok) fail(name, sql + ": " + whyFailed(r));
            else if (!db.hasUnsavedChanges()) fail(name, "returned false after " + sql);
            else pass(name);
        }
    }
    {
        std::string name = "loadAll clears it";
        Database db;
        if (setup(name, db, USERS)) {
            Result saved = db.saveAll(DIR + "/unsaved");
            run(db, "INSERT INTO users VALUES (9, 'x', 1)");
            Result loaded = db.loadAll(DIR + "/unsaved");
            if (!saved.ok || !loaded.ok) fail(name, "save or load failed: " + saved.message + " / " + loaded.message);
            else if (db.hasUnsavedChanges()) fail(name, "still true after loadAll");
            else pass(name);
        }
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 11. saveAll / loadAll (need storage.cpp) --\n";
    const std::string SAVE = DIR + "/save";
    {
        std::string name = "saveAll writes <dir>/<table>.csv for each table";
        Database db;
        if (makeUsers(name, db) && setup(name, db, {"CREATE TABLE empty (k TEXT)"})) {
            Result r = db.saveAll(SAVE);
            if (!r.ok) fail(name, "saveAll failed: " + r.message);
            else if (!fs::exists(SAVE + "/users.csv") || !fs::exists(SAVE + "/nums.csv") || !fs::exists(SAVE + "/empty.csv"))
                fail(name, "expected users.csv, nums.csv and empty.csv in " + SAVE);
            else if (readFile(SAVE + "/users.csv").rfind("id INT,name TEXT,age INT\n1,alice,30\n", 0) != 0)
                fail(name, "users.csv starts wrong: " + readFile(SAVE + "/users.csv").substr(0, 60));
            else pass(name);
        }
    }
    {
        std::string name = "loadAll brings back the same tables";
        Database db;
        Result r = db.loadAll(SAVE);
        if (!r.ok) fail(name, "loadAll failed: " + r.message);
        else if (db.tableNames() != std::vector<std::string>{"empty", "nums", "users"}) fail(name, "tables are " + join(db.tableNames()));
        else if (tableRows(db, "users") != ALL_USERS) fail(name, "users holds " + rowsToString(tableRows(db, "users")));
        else if (tableRows(db, "nums").size() != 5) fail(name, "nums has the wrong rows");
        else if (db.getTable("empty")->rows.size() != 0 || db.getTable("empty")->columns.size() != 1) fail(name, "empty is wrong");
        else if (db.getTable("users")->columns[2].type != ValueType::INT) fail(name, "age came back as TEXT");
        else pass(name);
    }
    {
        std::string name = "loaded tables work like normal";
        Database db;
        Result r = db.loadAll(SAVE);
        if (!r.ok) fail(name, "loadAll failed: " + r.message);
        else {
            checkSelect(name + ": WHERE + ORDER BY", db, "SELECT id FROM nums WHERE id > 2 ORDER BY id DESC", {"id"},
                        {{"100"}, {"10"}, {"9"}});
            checkError(name + ": duplicate key still caught", db, "INSERT INTO users VALUES (1, 'x', 1)");
            checkChange(name + ": INSERT", db, "INSERT INTO empty VALUES ('k1')", 1, "empty", {{"k1"}});
        }
    }
    {
        std::string name = "loadAll replaces the tables you had";
        Database db;
        if (setup(name, db, {"CREATE TABLE junk (id INT)", "CREATE TABLE users (x TEXT)"})) {
            Result r = db.loadAll(SAVE);
            if (!r.ok) fail(name, "loadAll failed: " + r.message);
            else if (db.hasTable("junk")) fail(name, "junk is still there");
            else if (tableRows(db, "users") != ALL_USERS) fail(name, "users wasn't replaced");
            else pass(name);
        }
    }
    {
        std::string name = "special characters survive a round trip";
        std::string dir = DIR + "/special";
        Database db;
        if (setup(name, db, {"CREATE TABLE t (k TEXT, v TEXT)",
                             "INSERT INTO t VALUES ('a,b', 'say \"hi\"')",
                             "INSERT INTO t VALUES ('  ', '')"})) {
            Result s = db.saveAll(dir);
            Database db2;
            Result l = db2.loadAll(dir);
            if (!s.ok || !l.ok) fail(name, "save or load failed: " + s.message + " / " + l.message);
            else if (tableRows(db2, "t") != tableRows(db, "t")) fail(name, "came back as " + rowsToString(tableRows(db2, "t")));
            else pass(name);
        }
    }
    {
        std::string name = "a dropped table stays dropped after save + load";
        Database db;
        Result l = db.loadAll(SAVE);
        Result d = run(db, "DROP TABLE nums");
        Result s = db.saveAll(SAVE);
        Database db2;
        Result l2 = db2.loadAll(SAVE);
        if (!l.ok || !d.ok || !s.ok || !l2.ok) fail(name, "a step failed: " + l.message + " / " + d.message + " / " + s.message + " / " + l2.message);
        else if (db2.hasTable("nums")) fail(name, "nums came back");
        else if (fs::exists(SAVE + "/nums.csv")) fail(name, "nums.csv is still in the folder");
        else pass(name);
    }
    {
        std::string name = "saveAll creates the folder";
        std::string dir = DIR + "/new/nested";
        Database db;
        if (setup(name, db, {"CREATE TABLE t (id INT)"})) {
            Result r = db.saveAll(dir);
            if (!r.ok) fail(name, "saveAll failed: " + r.message);
            else if (!fs::exists(dir + "/t.csv")) fail(name, "no t.csv");
            else pass(name);
        }
    }
    {
        std::string name = "save and load with no tables";
        std::string dir = DIR + "/none";
        Database db;
        Result s = db.saveAll(dir);
        Database db2;
        Result l = db2.loadAll(dir);
        if (!s.ok || !l.ok) fail(name, "save or load failed: " + s.message + " / " + l.message);
        else if (!db2.tableNames().empty()) fail(name, "got tables " + join(db2.tableNames()));
        else pass(name);
    }
    {
        std::string name = "loadAll ignores files that aren't .csv";
        std::string dir = DIR + "/mixed";
        fs::create_directories(dir);
        writeFile(dir + "/notes.txt", "not a table\n");
        writeFile(dir + "/a.csv", "id INT,v TEXT\n1,x\n");
        Database db;
        Result r = db.loadAll(dir);
        if (!r.ok) fail(name, "loadAll failed: " + r.message);
        else if (db.tableNames() != std::vector<std::string>{"a"}) fail(name, "tables are " + join(db.tableNames()));
        else pass(name);
    }
    {
        std::string name = "loadAll on a folder that doesn't exist fails and changes nothing";
        Database db;
        if (setup(name, db, USERS)) {
            std::string before = snapshot(db);
            Result r = db.loadAll(DIR + "/no_such_folder");
            if (r.ok) fail(name, "returned ok");
            else if (snapshot(db) != before) fail(name, "tables changed");
            else pass(name);
        }
    }
    {
        std::string name = "loadAll with one broken file fails and changes nothing";
        std::string dir = DIR + "/broken";
        fs::create_directories(dir);
        writeFile(dir + "/good.csv", "id INT,v TEXT\n1,x\n");
        writeFile(dir + "/bad.csv", "id INT,v TEXT\n1,x\n1,duplicate\n");
        Database db;
        if (setup(name, db, USERS)) {
            std::string before = snapshot(db);
            Result r = db.loadAll(dir);
            if (r.ok) fail(name, "returned ok");
            else if (snapshot(db) != before) fail(name, "tables changed");
            else pass(name);
        }
    }

    fs::remove_all(DIR);

    std::cout << "\n-- 12. formatResult --\n";
    {
        std::string name = "UTF-8 text doesn't throw the borders off";
        Database db;
        if (setup(name, db, {"CREATE TABLE t (id INT, name TEXT)",
                             "INSERT INTO t VALUES (1, 'José')",
                             "INSERT INTO t VALUES (2, 'Bob')"})) {
            std::string expected =
                "+----+------+\n"
                "| id | name |\n"
                "+----+------+\n"
                "|  1 | José |\n"
                "|  2 | Bob  |\n"
                "+----+------+\n"
                "(2 rows)";
            std::string actual = formatResult(run(db, "SELECT * FROM t"));
            if (actual != expected) fail(name, "expected:\n" + expected + "\n        got:\n" + actual);
            else pass(name);
        }
    }

    std::cout << "\n-- 13. missing keys --\n";
    {
        std::string name = "SELECT on a missing key says so";
        Database db;
        if (setup(name, db, USERS)) {
            Result r = run(db, "SELECT name FROM users WHERE id = 99");
            std::string expected =
                "+------+\n"
                "| name |\n"
                "+------+\n"
                "(no row with id = 99)";
            if (!r.ok) fail(name, whyFailed(r));
            else if (formatResult(r) != expected) fail(name, "expected:\n" + expected + "\n        got:\n" + formatResult(r));
            else pass(name);
        }
    }
    {
        std::string name = "a TEXT key is shown in quotes";
        Database db;
        if (setup(name, db, {"CREATE TABLE kv (k TEXT, v TEXT)"})) {
            Result r = run(db, "DELETE FROM kv WHERE k = 'zz'");
            if (r.ok) fail(name, "DELETE of a missing key should fail");
            else if (r.message != "no row with k = 'zz'") fail(name, "message was: " + r.message);
            else pass(name);
        }
    }
    {
        std::string name = "key exists but the rest of the WHERE doesn't match: 0 rows, not an error";
        Database db;
        if (setup(name, db, USERS))
            checkChange(name, db, "UPDATE users SET age = 1 WHERE id = 1 AND age > 100", 0, "users", ALL_USERS);
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
