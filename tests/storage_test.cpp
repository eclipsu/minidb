// Storage tests: escapeCsvField, parseCsv, saveTable, loadTable.
// Run with:  make test-storage
//
// Files are written to storage_test_tmp/ and it gets deleted at the end.

#include "../src/storage.h"
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
        "         (usually a bad pointer, reading past the end of a string/vector,\n"
        "         or std::stoll throwing on a bad number).\n"
        "         Run `make test-asan T=test-storage` to see the exact line.\n";
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

static const std::string DIR = "storage_test_tmp";

static Value I(long long n) { return Value{ValueType::INT, std::to_string(n)}; }
static Value T(const std::string& s) { return Value{ValueType::TEXT, s}; }

// Makes special characters visible in failure messages.
static std::string show(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '"') out += "\\\"";
        else out += c;
    }
    return out + "\"";
}

static std::string showRecords(const std::vector<std::vector<std::string>>& recs) {
    std::string out = "[";
    for (size_t i = 0; i < recs.size(); i++) {
        if (i) out += ", ";
        out += "[";
        for (size_t j = 0; j < recs[i].size(); j++) {
            if (j) out += ", ";
            out += show(recs[i][j]);
        }
        out += "]";
    }
    return out + "]";
}

static std::string rowToString(const Row& r) {
    std::string out = "{";
    for (size_t i = 0; i < r.size(); i++) {
        if (i) out += ", ";
        out += (r[i].type == ValueType::TEXT) ? show(r[i].text) : r[i].text;
    }
    return out + "}";
}

static bool sameRow(const Row& a, const Row& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i].type != b[i].type || a[i].text != b[i].text) return false;
    return true;
}

static bool sameColumns(const std::vector<Column>& a, const std::vector<Column>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i].type != b[i].type || a[i].name != b[i].name) return false;
    return true;
}

static std::string columnsToString(const std::vector<Column>& cols) {
    std::string out;
    for (const Column& c : cols) {
        if (!out.empty()) out += ",";
        out += c.name + (c.type == ValueType::INT ? " INT" : " TEXT");
    }
    return "[" + out + "]";
}

static std::string path(const std::string& name) { return DIR + "/" + name; }

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

static void checkEscape(const std::string& input, const std::string& expected) {
    std::string name = "escape " + show(input);
    std::string got = escapeCsvField(input);
    if (got != expected) fail(name, "expected " + show(expected) + " but got " + show(got));
    else pass(name);
}

static void checkParse(const std::string& name, const std::string& input,
                       const std::vector<std::vector<std::string>>& expected) {
    std::vector<std::vector<std::string>> recs;
    bool ok = parseCsv(input, recs);
    if (!ok) fail(name, "parseCsv returned false for " + show(input));
    else if (recs != expected) fail(name, "expected " + showRecords(expected) + " but got " + showRecords(recs));
    else pass(name);
}

// The columns we pre-fill before loading a broken file, to check they aren't touched.
static std::vector<Column> oldColumns() { return {Column{ValueType::TEXT, "untouched"}}; }

// A broken file must make loadTable return false and change nothing.
static void checkBroken(const std::string& name, const std::string& content) {
    std::string p = path("broken.csv");
    writeFile(p, content);
    std::vector<Column> cols = oldColumns();
    BTree tree(2);
    bool ok = loadTable(p, cols, tree);
    if (ok) fail(name, "loadTable returned true for " + show(content));
    else if (!sameColumns(cols, oldColumns())) fail(name, "columns changed to " + columnsToString(cols));
    else if (tree.size() != 0 || tree.getRoot() != nullptr) fail(name, "tree changed (size " + std::to_string(tree.size()) + ")");
    else pass(name);
}

// Saves, loads into a new tree, and compares columns and every row.
static void checkRoundTrip(const std::string& name, const std::vector<Column>& cols,
                           const std::vector<Row>& rows, int t = 3) {
    BTree tree(t);
    for (const Row& r : rows) tree.insert(r[0], r);
    std::string p = path("roundtrip.csv");
    if (!saveTable(p, cols, tree)) { fail(name, "saveTable returned false"); return; }

    std::vector<Column> loadedCols;
    BTree loaded(t);
    if (!loadTable(p, loadedCols, loaded)) { fail(name, "loadTable returned false on the saved file:\n        " + show(readFile(p))); return; }
    if (!sameColumns(loadedCols, cols)) { fail(name, "columns came back as " + columnsToString(loadedCols)); return; }

    auto before = tree.traverseInOrder();
    auto after = loaded.traverseInOrder();
    if (before.size() != after.size()) {
        fail(name, std::to_string(before.size()) + " rows saved but " + std::to_string(after.size()) + " loaded");
        return;
    }
    for (size_t i = 0; i < before.size(); i++) {
        if (!sameRow(before[i].second, after[i].second)) {
            fail(name, "row " + std::to_string(i) + " was " + rowToString(before[i].second) +
                       " but came back as " + rowToString(after[i].second));
            return;
        }
    }
    std::string why;
    if (!loaded.checkValid(&why)) { fail(name, "loaded tree is broken: " + why); return; }
    pass(name);
}

int main() {
    std::cout << std::unitbuf;
    std::signal(SIGSEGV, onCrash);
    std::signal(SIGABRT, onCrash);

    fs::remove_all(DIR);
    fs::create_directory(DIR);

    const std::vector<Column> users = {
        Column{ValueType::INT, "id"}, Column{ValueType::TEXT, "name"}, Column{ValueType::INT, "age"}};

    // ------------------------------------------------------------------
    std::cout << "\n-- 1. escapeCsvField --\n";
    checkEscape("bob", "bob");
    checkEscape("", "");
    checkEscape("  spaces  ", "  spaces  ");
    checkEscape("a,b", "\"a,b\"");
    checkEscape("say \"hi\"", "\"say \"\"hi\"\"\"");
    checkEscape("\"", "\"\"\"\"");
    checkEscape("line1\nline2", "\"line1\nline2\"");
    checkEscape("a\rb", "\"a\rb\"");
    checkEscape("a\r\nb", "\"a\r\nb\"");
    checkEscape("hi, \"bob\"", "\"hi, \"\"bob\"\"\"");

    // ------------------------------------------------------------------
    std::cout << "\n-- 2. parseCsv --\n";
    checkParse("empty text gives zero records", "", {});
    checkParse("one line, no newline at the end", "a,b,c", {{"a", "b", "c"}});
    checkParse("one line with a newline is ONE record", "a,b,c\n", {{"a", "b", "c"}});
    checkParse("two lines", "a,b\nc,d\n", {{"a", "b"}, {"c", "d"}});
    checkParse("\\r\\n line endings", "a,b\r\nc,d\r\n", {{"a", "b"}, {"c", "d"}});
    checkParse("\\r\\n with no newline at the end", "a,b\r\nc,d", {{"a", "b"}, {"c", "d"}});
    checkParse("empty fields", "a,,b\n", {{"a", "", "b"}});
    checkParse("comma at the end of a line", "a,\n", {{"a", ""}});
    checkParse("quoted field", "\"bob\",1\n", {{"bob", "1"}});
    checkParse("quoted empty field", "\"\",1\n", {{"", "1"}});
    checkParse("comma inside quotes", "\"a,b\",c\n", {{"a,b", "c"}});
    checkParse("\"\" inside quotes is one quote", "\"say \"\"hi\"\"\"\n", {{"say \"hi\""}});
    checkParse("newline inside quotes", "\"line1\nline2\",x\ny,z\n", {{"line1\nline2", "x"}, {"y", "z"}});
    checkParse("\\r\\n inside quotes is kept", "\"a\r\nb\"\r\n", {{"a\r\nb"}});
    checkParse("spaces are kept", " a , b \n", {{" a ", " b "}});
    {
        std::vector<std::vector<std::string>> recs;
        bool ok = parseCsv("\"never closed\n", recs);
        if (ok) fail("unclosed quote returns false", "parseCsv returned true");
        else pass("unclosed quote returns false");
    }
    {
        std::vector<std::vector<std::string>> recs;
        bool ok = parseCsv("a,\"b\nc,d\n", recs);
        if (ok) fail("unclosed quote in a later field returns false", "parseCsv returned true");
        else pass("unclosed quote in a later field returns false");
    }
    {
        std::vector<std::vector<std::string>> recs = {{"old", "stuff"}};
        bool ok = parseCsv("x\n", recs);
        if (!ok || recs != std::vector<std::vector<std::string>>{{"x"}})
            fail("records is cleared first", "got " + showRecords(recs));
        else pass("records is cleared first");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 3. saveTable --\n";
    {
        BTree tree(2);
        tree.insert(I(3), Row{I(3), T("carol"), I(30)});
        tree.insert(I(1), Row{I(1), T("alice"), I(20)});
        tree.insert(I(2), Row{I(2), T("bob"), I(25)});
        std::string p = path("users.csv");
        bool ok = saveTable(p, users, tree);
        std::string expected = "id INT,name TEXT,age INT\n1,alice,20\n2,bob,25\n3,carol,30\n";
        std::string got = readFile(p);
        if (!ok) fail("writes header and rows in key order", "saveTable returned false");
        else if (got != expected) fail("writes header and rows in key order", "file should be\n        " + show(expected) + "\n        but was\n        " + show(got));
        else pass("writes header and rows in key order");
    }
    {
        BTree tree(2);
        std::string p = path("empty.csv");
        bool ok = saveTable(p, users, tree);
        std::string got = readFile(p);
        if (!ok || got != "id INT,name TEXT,age INT\n") fail("empty table writes just the header", "file was " + show(got));
        else pass("empty table writes just the header");
    }
    {
        BTree tree(2);
        tree.insert(I(1), Row{I(1), T("hi, \"bob\""), I(-5)});
        tree.insert(I(2), Row{I(2), T("two\nlines"), I(0)});
        tree.insert(I(3), Row{I(3), T(""), I(7)});
        std::string p = path("escaped.csv");
        bool ok = saveTable(p, users, tree);
        std::string expected = "id INT,name TEXT,age INT\n1,\"hi, \"\"bob\"\"\",-5\n2,\"two\nlines\",0\n3,,7\n";
        std::string got = readFile(p);
        if (!ok || got != expected) fail("fields are escaped", "file should be\n        " + show(expected) + "\n        but was\n        " + show(got));
        else pass("fields are escaped");
    }
    {
        std::string p = path("overwrite.csv");
        writeFile(p, "old contents\n");
        BTree tree(2);
        tree.insert(I(1), Row{I(1), T("new"), I(1)});
        bool ok = saveTable(p, users, tree);
        std::string got = readFile(p);
        if (!ok || got != "id INT,name TEXT,age INT\n1,new,1\n") fail("replaces an existing file", "file was " + show(got));
        else pass("replaces an existing file");
    }
    {
        std::string p = path("notmp.csv");
        BTree tree(2);
        tree.insert(I(1), Row{I(1), T("a"), I(1)});
        bool ok = saveTable(p, users, tree);
        if (!ok) fail("no .tmp file left behind", "saveTable returned false");
        else if (fs::exists(p + ".tmp")) fail("no .tmp file left behind", p + ".tmp still exists");
        else pass("no .tmp file left behind");
    }
    {
        BTree tree(2);
        tree.insert(I(1), Row{I(1), T("a"), I(1)});
        bool ok = saveTable(path("no_such_folder/x.csv"), users, tree);
        if (ok) fail("folder that doesn't exist returns false", "saveTable returned true");
        else pass("folder that doesn't exist returns false");
    }
    {
        // A folder named <path>.tmp means the .tmp file can't be created.
        // If save really goes through .tmp, it has to fail and leave the old file alone.
        std::string name = "save goes through <path>.tmp (old file untouched if it can't)";
        std::string p = path("blocked.csv");
        writeFile(p, "old contents\n");
        fs::create_directory(p + ".tmp");
        BTree tree(2);
        tree.insert(I(1), Row{I(1), T("new"), I(1)});
        bool ok = saveTable(p, users, tree);
        std::string got = readFile(p);
        if (ok) fail(name, "saveTable returned true even though " + p + ".tmp is a folder");
        else if (got != "old contents\n") fail(name, "old file was changed to " + show(got));
        else pass(name);
        fs::remove_all(p + ".tmp");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 4. loadTable --\n";
    {
        std::string name = "loads columns and rows";
        std::string p = path("load.csv");
        writeFile(p, "id INT,name TEXT,age INT\n2,bob,25\n1,alice,20\n");
        std::vector<Column> cols;
        BTree tree(2);
        bool ok = loadTable(p, cols, tree);
        const Row* r1 = ok ? tree.find(I(1)) : nullptr;
        const Row* r2 = ok ? tree.find(I(2)) : nullptr;
        if (!ok) fail(name, "loadTable returned false");
        else if (!sameColumns(cols, users)) fail(name, "columns were " + columnsToString(cols));
        else if (tree.size() != 2) fail(name, "tree has " + std::to_string(tree.size()) + " rows, should be 2");
        else if (!r1 || !sameRow(*r1, Row{I(1), T("alice"), I(20)})) fail(name, "row 1 is wrong: " + (r1 ? rowToString(*r1) : "missing"));
        else if (!r2 || !sameRow(*r2, Row{I(2), T("bob"), I(25)})) fail(name, "row 2 is wrong: " + (r2 ? rowToString(*r2) : "missing"));
        else pass(name);
    }
    {
        std::string name = "header only gives an empty table";
        std::string p = path("load_empty.csv");
        writeFile(p, "id INT,name TEXT\n");
        std::vector<Column> cols;
        BTree tree(2);
        bool ok = loadTable(p, cols, tree);
        std::vector<Column> expected = {Column{ValueType::INT, "id"}, Column{ValueType::TEXT, "name"}};
        if (!ok || !sameColumns(cols, expected) || tree.size() != 0) fail(name, "got columns " + columnsToString(cols) + ", " + std::to_string(tree.size()) + " rows");
        else pass(name);
    }
    {
        std::string name = "\\r\\n file with quoted fields";
        std::string p = path("load_crlf.csv");
        writeFile(p, "id INT,note TEXT\r\n1,\"a, b\"\r\n2,\"say \"\"hi\"\"\"\r\n");
        std::vector<Column> cols;
        BTree tree(2);
        bool ok = loadTable(p, cols, tree);
        const Row* r1 = ok ? tree.find(I(1)) : nullptr;
        const Row* r2 = ok ? tree.find(I(2)) : nullptr;
        if (!ok) fail(name, "loadTable returned false");
        else if (!r1 || !sameRow(*r1, Row{I(1), T("a, b")})) fail(name, "row 1 is wrong: " + (r1 ? rowToString(*r1) : "missing"));
        else if (!r2 || !sameRow(*r2, Row{I(2), T("say \"hi\"")})) fail(name, "row 2 is wrong: " + (r2 ? rowToString(*r2) : "missing"));
        else pass(name);
    }
    {
        std::string name = "negative INT values";
        std::string p = path("load_neg.csv");
        writeFile(p, "id INT,x INT\n-5,-100\n0,0\n");
        std::vector<Column> cols;
        BTree tree(2);
        bool ok = loadTable(p, cols, tree);
        const Row* r = ok ? tree.find(I(-5)) : nullptr;
        if (!ok || !r || !sameRow(*r, Row{I(-5), I(-100)})) fail(name, ok ? "row -5 is wrong" : "loadTable returned false");
        else pass(name);
    }
    {
        std::string name = "TEXT key column";
        std::string p = path("load_text.csv");
        writeFile(p, "name TEXT,age INT\nzoe,30\namy,22\n");
        std::vector<Column> cols;
        BTree tree(2);
        bool ok = loadTable(p, cols, tree);
        auto all = ok ? tree.traverseInOrder() : std::vector<std::pair<Value, Row>>{};
        if (!ok) fail(name, "loadTable returned false");
        else if (all.size() != 2 || all[0].first.text != "amy" || all[0].first.type != ValueType::TEXT)
            fail(name, "rows should be amy, zoe in that order");
        else pass(name);
    }
    {
        std::string name = "tree that isn't empty returns false and isn't changed";
        std::string p = path("load.csv");   // a good file from above
        std::vector<Column> cols = oldColumns();
        BTree tree(2);
        tree.insert(I(99), Row{I(99), T("x"), I(1)});
        bool ok = loadTable(p, cols, tree);
        if (ok) fail(name, "loadTable returned true");
        else if (tree.size() != 1 || !tree.find(I(99)) || tree.find(I(1))) fail(name, "tree was changed");
        else if (!sameColumns(cols, oldColumns())) fail(name, "columns were changed");
        else pass(name);
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 5. broken files (must return false AND change nothing) --\n";
    {
        std::vector<Column> cols = oldColumns();
        BTree tree(2);
        bool ok = loadTable(path("does_not_exist.csv"), cols, tree);
        if (ok || !sameColumns(cols, oldColumns()) || tree.size() != 0) fail("file that doesn't exist", "loadTable returned true or changed something");
        else pass("file that doesn't exist");
    }
    checkBroken("empty file", "");
    checkBroken("unclosed quote", "id INT,name TEXT\n1,\"bob\n");
    checkBroken("unknown type", "id INT,price FLOAT\n1,2\n");
    checkBroken("column with no type", "id INT,name\n1,bob\n");
    checkBroken("column with no name", "id INT, TEXT\n1,bob\n");
    checkBroken("empty header line", "\n1,bob\n");
    checkBroken("row with too few fields", "id INT,name TEXT\n1,bob\n2\n");
    checkBroken("row with too many fields", "id INT,name TEXT\n1,bob\n2,amy,extra\n");
    checkBroken("INT that is a word", "id INT,age INT\n1,twenty\n");
    checkBroken("INT with a decimal point", "id INT,age INT\n1,2.5\n");
    checkBroken("INT that is empty", "id INT,age INT\n1,\n");
    checkBroken("INT that is just a minus", "id INT,age INT\n1,-\n");
    checkBroken("INT with a space", "id INT,age INT\n1, 5\n");
    checkBroken("INT too big for a long long", "id INT,age INT\n1,5\n99999999999999999999,6\n");
    checkBroken("duplicate key", "id INT,name TEXT\n1,bob\n2,amy\n1,carl\n");
    checkBroken("duplicate key written differently (9 and 09)", "id INT,name TEXT\n9,bob\n09,amy\n");
    checkBroken("duplicate TEXT key", "name TEXT,age INT\nbob,1\nbob,2\n");
    checkBroken("bad row at the very end (nothing loaded before it)",
                "id INT,name TEXT\n1,a\n2,b\n3,c\n4,d\n5,e\n6,f\n7,g\n8,h\nnine,i\n");

    // ------------------------------------------------------------------
    std::cout << "\n-- 6. save then load --\n";
    checkRoundTrip("simple rows", users, {
        Row{I(1), T("alice"), I(20)}, Row{I(2), T("bob"), I(25)}});
    checkRoundTrip("commas", users, {
        Row{I(1), T("a,b,c"), I(1)}, Row{I(2), T(","), I(2)}, Row{I(3), T(",start and end,"), I(3)}});
    checkRoundTrip("quotes", users, {
        Row{I(1), T("say \"hi\""), I(1)}, Row{I(2), T("\""), I(2)}, Row{I(3), T("\"\"\""), I(3)}});
    checkRoundTrip("newlines", users, {
        Row{I(1), T("line1\nline2"), I(1)}, Row{I(2), T("\n"), I(2)}, Row{I(3), T("a\r\nb"), I(3)}});
    checkRoundTrip("empty strings", users, {
        Row{I(1), T(""), I(1)}, Row{I(2), T(""), I(2)}});
    checkRoundTrip("spaces at the start and end", users, {
        Row{I(1), T("  padded  "), I(1)}});
    checkRoundTrip("everything at once", users, {
        Row{I(-3), T("hi, \"bob\"\nbye"), I(-1)}, Row{I(0), T(",\"\n\r"), I(0)}});
    checkRoundTrip("TEXT keys with special characters", {Column{ValueType::TEXT, "k"}, Column{ValueType::TEXT, "v"}}, {
        Row{T("a,b"), T("1")}, Row{T("\"q\""), T("2")}, Row{T("x\ny"), T("3")}, Row{T(""), T("")}});
    checkRoundTrip("one column table", {Column{ValueType::INT, "id"}}, {
        Row{I(1)}, Row{I(2)}, Row{I(3)}});
    {
        std::vector<Row> rows;
        for (int k = 1000; k >= 1; k--)
            rows.push_back(Row{I(k), T("name, \"" + std::to_string(k) + "\"\n"), I(k % 90)});
        checkRoundTrip("1000 rows", users, rows);
    }
    {
        std::string name = "save, load, save again gives the same file";
        BTree tree(3);
        for (int k = 1; k <= 50; k++) tree.insert(I(k), Row{I(k), T("v," + std::to_string(k) + "\"x\""), I(-k)});
        std::string p1 = path("first.csv"), p2 = path("second.csv");
        std::vector<Column> cols;
        BTree loaded(3);
        bool ok = saveTable(p1, users, tree) && loadTable(p1, cols, loaded) && saveTable(p2, cols, loaded);
        if (!ok) fail(name, "a save or load returned false");
        else if (readFile(p1) != readFile(p2)) fail(name, "the two files are different");
        else pass(name);
    }

    fs::remove_all(DIR);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
