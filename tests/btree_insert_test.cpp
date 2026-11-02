// B-Tree tests for Hushay: find, insert (+ splitting), update, traverseInOrder.
// Run with:  make test-insert
// Work top to bottom: each section builds on the one before.

#include "../src/btree.h"
#include <algorithm>
#include <csignal>
#include <iostream>
#include <unistd.h>
#include <random>
#include <string>
#include <vector>

static void onCrash(int) {
    const char msg[] =
        "\n  CRASH  the test right after the last PASS/FAIL line above crashed\n"
        "         (usually a bad pointer, or reading past the end of a vector).\n"
        "         Run `make test-asan` to see the exact line in your code.\n";
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

static Value I(int n) { return Value{ValueType::INT, std::to_string(n)}; }
static Value T(const std::string& s) { return Value{ValueType::TEXT, s}; }

// The row we store for key n: {n, 'name<n>'}
static Row rowFor(int n) { return Row{I(n), T("name" + std::to_string(n))}; }

static std::string rowToString(const Row& r) {
    std::string out = "{";
    for (size_t i = 0; i < r.size(); i++) {
        if (i) out += ", ";
        out += (r[i].type == ValueType::TEXT) ? "'" + r[i].text + "'" : r[i].text;
    }
    return out + "}";
}

static bool sameRow(const Row& a, const Row& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i].type != b[i].type || a[i].text != b[i].text) return false;
    return true;
}

// All keys in order, as text like "1 2 3"
static std::string keysInOrder(const BTree& tree) {
    std::string out;
    for (const auto& kv : tree.traverseInOrder()) {
        if (!out.empty()) out += " ";
        out += kv.first.text;
    }
    return out;
}

static std::string range(int from, int to) {
    std::string out;
    for (int i = from; i <= to; i++) {
        if (i > from) out += " ";
        out += std::to_string(i);
    }
    return out;
}

// Returns "" if the tree passes checkValid, otherwise the reason.
static std::string invalidReason(const BTree& tree) {
    std::string why;
    if (tree.checkValid(&why)) return "";
    return why;
}

int main() {
    std::cout << std::unitbuf;
    std::signal(SIGSEGV, onCrash);
    std::signal(SIGABRT, onCrash);

    // ------------------------------------------------------------------
    std::cout << "\n-- 1. empty tree --\n";
    {
        BTree tree(3);
        if (tree.size() != 0)                fail("empty tree has size 0", "size() was " + std::to_string(tree.size()));
        else                                 pass("empty tree has size 0");
        if (tree.find(I(1)) != nullptr)      fail("find on empty tree returns nullptr", "got a row back");
        else                                 pass("find on empty tree returns nullptr");
        if (!tree.traverseInOrder().empty()) fail("traverse on empty tree is empty", "got " + keysInOrder(tree));
        else                                 pass("traverse on empty tree is empty");
        if (tree.update(I(1), rowFor(1)))    fail("update on empty tree returns false", "returned true");
        else                                 pass("update on empty tree returns false");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 2. find + insert, no splitting yet --\n";
    {
        BTree tree(3);
        if (!tree.insert(I(5), rowFor(5))) fail("insert into empty tree returns true", "returned false");
        else                               pass("insert into empty tree returns true");

        const Row* r = tree.find(I(5));
        if (r == nullptr)                       fail("find the key you inserted", "find returned nullptr");
        else if (!sameRow(*r, rowFor(5)))       fail("find the key you inserted", "wrong row: " + rowToString(*r));
        else                                    pass("find the key you inserted");

        if (tree.find(I(6)) != nullptr) fail("find a missing key returns nullptr", "got a row back");
        else                            pass("find a missing key returns nullptr");

        if (tree.size() != 1)   fail("size is 1 after one insert", "size() was " + std::to_string(tree.size()));
        else                    pass("size is 1 after one insert");
        if (tree.height() != 1) fail("height is 1 after one insert", "height() was " + std::to_string(tree.height()));
        else                    pass("height is 1 after one insert");
    }
    {
        // t = 3 -> a node holds up to 5 keys, so 5 inserts fit in the root.
        BTree tree(3);
        for (int k : {30, 10, 50, 20, 40}) tree.insert(I(k), rowFor(k));
        std::string got = keysInOrder(tree);
        if (got != "10 20 30 40 50") fail("keys come out sorted (5 keys, one node)", "got [" + got + "]");
        else                         pass("keys come out sorted (5 keys, one node)");
        if (tree.height() != 1)      fail("5 keys with t=3 still fit in one node", "height() was " + std::to_string(tree.height()));
        else                         pass("5 keys with t=3 still fit in one node");
        std::string bad = invalidReason(tree);
        if (!bad.empty())            fail("tree is valid", bad);
        else                         pass("tree is valid");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 3. duplicates --\n";
    {
        BTree tree(3);
        tree.insert(I(7), rowFor(7));
        Row other = {I(7), T("someone else")};
        if (tree.insert(I(7), other)) fail("inserting a duplicate key returns false", "returned true");
        else                          pass("inserting a duplicate key returns false");
        const Row* r = tree.find(I(7));
        if (!r || !sameRow(*r, rowFor(7))) fail("duplicate does not overwrite the original row", r ? "row is now " + rowToString(*r) : "key disappeared");
        else                               pass("duplicate does not overwrite the original row");
        if (tree.size() != 1)              fail("duplicate does not change size", "size() was " + std::to_string(tree.size()));
        else                               pass("duplicate does not change size");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 4. splitting (t = 2: a node holds at most 3 keys) --\n";
    {
        BTree tree(2);
        for (int k = 1; k <= 3; k++) tree.insert(I(k), rowFor(k));
        if (tree.height() != 1) fail("3 keys fit in one node", "height() was " + std::to_string(tree.height()));
        else                    pass("3 keys fit in one node");

        tree.insert(I(4), rowFor(4));
        if (tree.height() != 2)            fail("4th key splits the root (height becomes 2)", "height() was " + std::to_string(tree.height()));
        else                               pass("4th key splits the root (height becomes 2)");
        std::string bad = invalidReason(tree);
        if (!bad.empty())                  fail("tree is valid after the first split", bad);
        else                               pass("tree is valid after the first split");
        if (keysInOrder(tree) != "1 2 3 4") fail("all 4 keys still there, in order", "got [" + keysInOrder(tree) + "]");
        else                                pass("all 4 keys still there, in order");

        bool allFound = true;
        for (int k = 1; k <= 4; k++) if (!tree.find(I(k))) allFound = false;
        if (!allFound) fail("find works across children after a split", "a key could not be found");
        else           pass("find works across children after a split");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 5. many inserts --\n";
    auto bigTest = [](const std::string& name, int t, std::vector<int> keys) {
        BTree tree(t);
        for (int k : keys) {
            if (!tree.insert(I(k), rowFor(k))) return fail(name, "insert(" + std::to_string(k) + ") returned false");
            std::string bad = invalidReason(tree);
            if (!bad.empty()) return fail(name, "after inserting " + std::to_string(k) + ": " + bad);
        }
        std::sort(keys.begin(), keys.end());
        if (tree.size() != keys.size())
            return fail(name, "size() was " + std::to_string(tree.size()) + ", expected " + std::to_string(keys.size()));
        if (keysInOrder(tree) != range(keys.front(), keys.back()))
            return fail(name, "traverseInOrder is not 1..N in order");
        for (int k : keys) {
            const Row* r = tree.find(I(k));
            if (!r)                     return fail(name, "find(" + std::to_string(k) + ") returned nullptr");
            if (!sameRow(*r, rowFor(k))) return fail(name, "find(" + std::to_string(k) + ") has the wrong row " + rowToString(*r));
        }
        pass(name);
    };
    {
        std::vector<int> asc, desc, shuffled;
        for (int i = 1; i <= 100; i++) asc.push_back(i);
        for (int i = 100; i >= 1; i--) desc.push_back(i);
        for (int i = 1; i <= 500; i++) shuffled.push_back(i);
        std::mt19937 rng(42);
        std::shuffle(shuffled.begin(), shuffled.end(), rng);

        bigTest("100 keys ascending, t=2",   2, asc);
        bigTest("100 keys descending, t=2",  2, desc);
        bigTest("500 keys shuffled, t=2",    2, shuffled);
        bigTest("500 keys shuffled, t=3",    3, shuffled);
        bigTest("500 keys shuffled, t=5",    5, shuffled);
    }
    {
        BTree tree(2);
        for (int k = 1; k <= 1000; k++) tree.insert(I(k), rowFor(k));
        // A B-Tree with t=2 and 1000 keys is at most 10 levels deep.
        if (tree.height() > 10) fail("height stays small (1000 keys, t=2, height <= 10)", "height() was " + std::to_string(tree.height()));
        else                    pass("height stays small (1000 keys, t=2, height <= 10)");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 6. key ordering --\n";
    {
        BTree tree(2);
        for (int k : {100, 9, 10, -5, 0}) tree.insert(I(k), rowFor(k));
        std::string got = keysInOrder(tree);
        if (got != "-5 0 9 10 100") fail("INT keys sort as numbers (9 < 10 < 100)", "got [" + got + "]");
        else                        pass("INT keys sort as numbers (9 < 10 < 100)");
    }
    {
        BTree tree(2);
        for (std::string s : {"cal", "amy", "dan", "bob", "eve"}) tree.insert(T(s), Row{T(s)});
        std::string got = keysInOrder(tree);
        if (got != "amy bob cal dan eve") fail("TEXT keys sort alphabetically", "got [" + got + "]");
        else                              pass("TEXT keys sort alphabetically");
        if (!tree.find(T("dan")))         fail("find a TEXT key", "find('dan') returned nullptr");
        else                              pass("find a TEXT key");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 7. update --\n";
    {
        BTree tree(2);
        for (int k = 1; k <= 20; k++) tree.insert(I(k), rowFor(k));
        Row newRow = {I(13), T("changed")};
        if (!tree.update(I(13), newRow)) fail("update an existing key returns true", "returned false");
        else                             pass("update an existing key returns true");
        const Row* r = tree.find(I(13));
        if (!r || !sameRow(*r, newRow))  fail("find returns the new row after update", r ? "row is " + rowToString(*r) : "key disappeared");
        else                             pass("find returns the new row after update");
        if (tree.size() != 20)           fail("update does not change size", "size() was " + std::to_string(tree.size()));
        else                             pass("update does not change size");
        if (tree.update(I(99), rowFor(99))) fail("update a missing key returns false", "returned true");
        else                                pass("update a missing key returns false");
        if (tree.find(I(99)) != nullptr)    fail("update a missing key does not insert it", "key 99 now exists");
        else                                pass("update a missing key does not insert it");
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 8. traverseInOrder returns the rows too --\n";
    {
        BTree tree(2);
        for (int k : {8, 3, 5, 1, 9, 2, 7}) tree.insert(I(k), rowFor(k));
        auto all = tree.traverseInOrder();
        bool ok = all.size() == 7;
        for (const auto& kv : all) if (!sameRow(kv.second, rowFor(std::stoi(kv.first.text)))) ok = false;
        if (!ok) fail("every pair has the matching row", "a row does not belong to its key");
        else     pass("every pair has the matching row");
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
