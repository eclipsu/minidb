// B-Tree tests for Z: remove (+ borrow and merge).
// Run with:  make test-remove
//
// These tests build trees BY HAND and walk them directly, so they do NOT
// depend on insert, find or traverseInOrder. You can start right away.
//
// Pictures use t = 2 (min 1 key per non-root node, max 3) unless they say t = 3.

#include "../src/btree.h"
#include <algorithm>
#include <csignal>
#include <iostream>
#include <unistd.h>
#include <random>
#include <string>
#include <vector>

// If a test crashes (segmentation fault), say where instead of dying silently.
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

// ---------- building trees by hand ----------

static Value I(int n) { return Value{ValueType::INT, std::to_string(n)}; }
static Row rowFor(int n) { return Row{I(n), Value{ValueType::TEXT, "name" + std::to_string(n)}}; }

static BTreeNode* leaf(const std::vector<int>& keys) {
    BTreeNode* n = new BTreeNode();
    n->leaf = true;
    for (int k : keys) { n->keys.push_back(I(k)); n->rows.push_back(rowFor(k)); }
    return n;
}

static BTreeNode* node(const std::vector<int>& keys, const std::vector<BTreeNode*>& children) {
    BTreeNode* n = leaf(keys);
    n->leaf = false;
    n->children = children;
    return n;
}

// A tree of height h where EVERY node has the minimum t - 1 keys
// (so almost every remove has to borrow or merge). Keys are 1, 2, 3, ...
static BTreeNode* buildMinimal(int t, int h, int& next) {
    if (h == 1) {
        std::vector<int> keys;
        for (int i = 0; i < t - 1; i++) keys.push_back(next++);
        return leaf(keys);
    }
    std::vector<BTreeNode*> kids;
    std::vector<int> keys;
    for (int i = 0; i < t; i++) {
        kids.push_back(buildMinimal(t, h - 1, next));
        if (i < t - 1) keys.push_back(next++);
    }
    return node(keys, kids);
}

// A tree of height h where EVERY node is full (2t - 1 keys).
static BTreeNode* buildFull(int t, int h, int& next) {
    if (h == 1) {
        std::vector<int> keys;
        for (int i = 0; i < 2 * t - 1; i++) keys.push_back(next++);
        return leaf(keys);
    }
    std::vector<BTreeNode*> kids;
    std::vector<int> keys;
    for (int i = 0; i < 2 * t; i++) {
        kids.push_back(buildFull(t, h - 1, next));
        if (i < 2 * t - 1) keys.push_back(next++);
    }
    return node(keys, kids);
}

// ---------- walking trees by hand (no traverseInOrder needed) ----------

static void walk(const BTreeNode* n, std::vector<int>& keys, bool& rowsOk) {
    if (!n) return;
    for (size_t i = 0; i < n->keys.size(); i++) {
        if (!n->leaf && i < n->children.size()) walk(n->children[i], keys, rowsOk);
        int k = std::stoi(n->keys[i].text);
        keys.push_back(k);
        if (i >= n->rows.size() || n->rows[i].size() != 2 || n->rows[i][0].text != n->keys[i].text) rowsOk = false;
    }
    if (!n->leaf && !n->children.empty() && n->children.size() > n->keys.size())
        walk(n->children.back(), keys, rowsOk);
}

static std::string toString(const std::vector<int>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) out += (i ? " " : "") + std::to_string(v[i]);
    return out;
}

static std::string keysOf(const BTree& tree, bool& rowsOk) {
    std::vector<int> keys;
    rowsOk = true;
    walk(tree.getRoot(), keys, rowsOk);
    return toString(keys);
}

static std::string rootKeys(const BTree& tree) {
    const BTreeNode* r = tree.getRoot();
    if (!r) return "(empty)";
    std::vector<int> v;
    for (const Value& k : r->keys) v.push_back(std::stoi(k.text));
    return toString(v);
}

// Load `root` into a tree, remove `key`, and check everything.
//   expectKeys:   all keys afterwards, in order
//   expectHeight: tree height afterwards
//   expectRoot:   root's keys afterwards ("" = don't check)
static void removeTest(const std::string& name, int t, BTreeNode* root, int key, bool expectReturn,
                       const std::string& expectKeys, int expectHeight, const std::string& expectRoot = "") {
    BTree tree(t);
    tree.loadTreeForTesting(root);
    size_t before = tree.size();

    bool got = tree.remove(I(key));
    if (got != expectReturn)
        return fail(name, std::string("remove(") + std::to_string(key) + ") returned " + (got ? "true" : "false"));

    std::string why;
    if (!tree.checkValid(&why)) return fail(name, "tree is broken after remove: " + why);

    bool rowsOk;
    std::string keys = keysOf(tree, rowsOk);
    if (keys != expectKeys) return fail(name, "keys should be [" + expectKeys + "] but were [" + keys + "]");
    if (!rowsOk)            return fail(name, "a row got separated from its key (move rows together with keys)");

    size_t wantSize = expectReturn ? before - 1 : before;
    if (tree.size() != wantSize)
        return fail(name, "size() should be " + std::to_string(wantSize) + " but was " + std::to_string(tree.size()));
    if (tree.height() != expectHeight)
        return fail(name, "height should be " + std::to_string(expectHeight) + " but was " + std::to_string(tree.height()));
    if (!expectRoot.empty() && rootKeys(tree) != expectRoot)
        return fail(name, "root should be [" + expectRoot + "] but was [" + rootKeys(tree) + "]");
    pass(name);
}

// Remove every key in `order` one at a time, checking the tree after each step.
static void removeAllTest(const std::string& name, int t, BTreeNode* root, std::vector<int> order) {
    BTree tree(t);
    tree.loadTreeForTesting(root);
    std::vector<int> remaining = order;
    std::sort(remaining.begin(), remaining.end());

    for (int k : order) {
        if (!tree.remove(I(k))) return fail(name, "remove(" + std::to_string(k) + ") returned false");
        remaining.erase(std::find(remaining.begin(), remaining.end(), k));

        std::string why;
        if (!tree.checkValid(&why)) return fail(name, "after removing " + std::to_string(k) + ": " + why);
        bool rowsOk;
        if (keysOf(tree, rowsOk) != toString(remaining))
            return fail(name, "after removing " + std::to_string(k) + ", keys are wrong: [" + keysOf(tree, rowsOk) + "]");
        if (!rowsOk) return fail(name, "after removing " + std::to_string(k) + ", a row got separated from its key");
        if (tree.remove(I(k))) return fail(name, "removing " + std::to_string(k) + " a second time returned true");
    }
    if (tree.getRoot() != nullptr) return fail(name, "after removing everything, root should be nullptr");
    if (tree.size() != 0 || tree.height() != 0) return fail(name, "after removing everything, size and height should be 0");
    pass(name);
}

int main() {
    std::cout << std::unitbuf;          // print each line right away
    std::signal(SIGSEGV, onCrash);
    std::signal(SIGABRT, onCrash);

    // ------------------------------------------------------------------
    std::cout << "\n-- 1. easy cases --\n";
    {
        BTree tree(2);
        if (tree.remove(I(1))) fail("remove from an empty tree returns false", "returned true");
        else                   pass("remove from an empty tree returns false");
    }
    //   [10 20 30]  remove 99
    removeTest("missing key returns false, nothing changes", 2, leaf({10, 20, 30}), 99, false,
               "10 20 30", 1);
    //            [10]               remove 7 (not there)
    //          ╱      ╲
    //       [5]        [15]
    removeTest("missing key in a bigger tree", 2, node({10}, {leaf({5}), leaf({15})}), 7, false,
               "5 10 15", 2);
    //   [10 20 30]  remove 20
    removeTest("remove from a root leaf", 2, leaf({10, 20, 30}), 20, true,
               "10 30", 1);
    //   [10]  remove 10 -> empty tree
    removeTest("remove the last key -> empty tree (root = nullptr)", 2, leaf({10}), 10, true,
               "", 0, "(empty)");

    // ------------------------------------------------------------------
    std::cout << "\n-- 2. key is in a leaf that has extra keys (case 1) --\n";
    //            [10]
    //          ╱      ╲
    //     [5 6]        [15 16]       remove 6
    removeTest("leaf has more than t-1 keys", 2, node({10}, {leaf({5, 6}), leaf({15, 16})}), 6, true,
               "5 10 15 16", 2, "10");

    // ------------------------------------------------------------------
    std::cout << "\n-- 3. key is in an internal node (cases 2a, 2b, 2c) --\n";
    //            [10]
    //          ╱      ╲
    //     [5 6]        [15]          remove 10: left child has t keys -> predecessor 6
    removeTest("2a: use the predecessor", 2, node({10}, {leaf({5, 6}), leaf({15})}), 10, true,
               "5 6 15", 2, "6");
    //            [10]
    //          ╱      ╲
    //       [5]        [15 16]       remove 10: left is minimal, right has t keys -> successor 15
    removeTest("2b: use the successor", 2, node({10}, {leaf({5}), leaf({15, 16})}), 10, true,
               "5 15 16", 2, "15");
    //            [10 20]
    //          ╱    |    ╲
    //       [5]   [15]   [25]        remove 10: both children minimal -> merge [5 10 15], then remove 10
    removeTest("2c: both children minimal -> merge", 2,
               node({10, 20}, {leaf({5}), leaf({15}), leaf({25})}), 10, true,
               "5 15 20 25", 2, "20");

    // ------------------------------------------------------------------
    std::cout << "\n-- 4. going down into a minimal child (case 3a borrow, 3b merge) --\n";
    //            [10]
    //          ╱      ╲
    //    [3 5]         [15]          remove 15: [15] is minimal, left sibling has extra -> borrow
    removeTest("3a: borrow from the left sibling", 2, node({10}, {leaf({3, 5}), leaf({15})}), 15, true,
               "3 5 10", 2, "5");
    //            [10]
    //          ╱      ╲
    //       [5]        [15 20]       remove 5: [5] is minimal, right sibling has extra -> borrow
    removeTest("3a: borrow from the right sibling", 2, node({10}, {leaf({5}), leaf({15, 20})}), 5, true,
               "10 15 20", 2, "15");
    //            [10 20]
    //          ╱    |    ╲
    //       [5]   [15]   [25]        remove 5: no sibling to borrow from -> merge [5 10 15]
    removeTest("3b: merge with a sibling", 2, node({10, 20}, {leaf({5}), leaf({15}), leaf({25})}), 5, true,
               "10 15 20 25", 2, "20");
    //            [10 20]
    //          ╱    |    ╲
    //       [5]   [15]   [25]        remove 25: last child, merge with the LEFT sibling
    removeTest("3b: merge when there is no right sibling", 2,
               node({10, 20}, {leaf({5}), leaf({15}), leaf({25})}), 25, true,
               "5 10 15 20", 2, "10");

    // ------------------------------------------------------------------
    std::cout << "\n-- 5. the tree gets shorter --\n";
    //            [10]
    //          ╱      ╲
    //       [5]        [15]          remove 5: merge into [10 15], root becomes that leaf
    removeTest("root shrinks: height 2 -> 1", 2, node({10}, {leaf({5}), leaf({15})}), 5, true,
               "10 15", 1, "10 15");
    //            [10]  remove 10: merge, root shrinks
    removeTest("remove the root key itself, root shrinks", 2, node({10}, {leaf({5}), leaf({15})}), 10, true,
               "5 15", 1, "5 15");
    {
        int next = 1;
        //                 [4]
        //              ╱       ╲
        //          [2]           [6]
        //         ╱   ╲         ╱   ╲
        //       [1]   [3]     [5]   [7]       remove 1: merges all the way up, height 3 -> 2
        removeTest("height 3 -> 2 (merges cascade up)", 2, buildMinimal(2, 3, next), 1, true,
                   "2 3 4 5 6 7", 2);
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 6. borrowing moves children too (internal nodes) --\n";
    {
        //                    [8]
        //              ╱            ╲
        //        [4]                 [12 16]
        //       ╱   ╲               ╱   |    ╲
        //     [2]   [6]          [10] [14]   [18]
        // remove 2: going down into [4] (minimal), right sibling [12 16] has extra
        //           -> borrow, and a CHILD ([10]) must move from right to left
        BTreeNode* root = node({8}, {
            node({4},      {leaf({2}), leaf({6})}),
            node({12, 16}, {leaf({10}), leaf({14}), leaf({18})}),
        });
        removeTest("internal borrow from the right moves a child", 2, root, 2, true,
                   "4 6 8 10 12 14 16 18", 3);
    }
    {
        //                    [10]
        //              ╱            ╲
        //        [4 8]                [14]
        //       ╱  |  ╲              ╱    ╲
        //     [2] [6] [9]          [12]   [16]
        // remove 16: going down into [14] (minimal), left sibling [4 8] has extra
        //            -> borrow, and a CHILD ([9]) must move from left to right
        BTreeNode* root = node({10}, {
            node({4, 8}, {leaf({2}), leaf({6}), leaf({9})}),
            node({14},   {leaf({12}), leaf({16})}),
        });
        removeTest("internal borrow from the left moves a child", 2, root, 16, true,
                   "2 4 6 8 9 10 12 14", 3);
    }

    // ------------------------------------------------------------------
    std::cout << "\n-- 7. t = 3 (min 2 keys, max 5) --\n";
    //              [30]
    //           ╱        ╲
    //   [10 20]            [40 50]       remove 40: right child minimal, left minimal -> merge
    removeTest("t=3 merge", 3, node({30}, {leaf({10, 20}), leaf({40, 50})}), 40, true,
               "10 20 30 50", 1, "10 20 30 50");
    //              [30]
    //           ╱        ╲
    //   [10 20 25]         [40 50]       remove 50: borrow from the left
    removeTest("t=3 borrow", 3, node({30}, {leaf({10, 20, 25}), leaf({40, 50})}), 50, true,
               "10 20 25 30 40", 2, "25");

    // ------------------------------------------------------------------
    std::cout << "\n-- 8. remove everything, checking the tree after every step --\n";
    auto orders = [](int n, int seed) {
        std::vector<std::vector<int>> out(3);
        for (int i = 1; i <= n; i++) out[0].push_back(i);      // ascending
        for (int i = n; i >= 1; i--) out[1].push_back(i);      // descending
        out[2] = out[0];
        std::mt19937 rng(seed);
        std::shuffle(out[2].begin(), out[2].end(), rng);        // shuffled
        return out;
    };
    const char* names[] = {"ascending", "descending", "shuffled"};

    // t=2, height 4, every node minimal: 15 keys
    { auto o = orders(15, 1);
      for (int i = 0; i < 3; i++) { int next = 1;
        removeAllTest(std::string("t=2 minimal tree, 15 keys, ") + names[i], 2, buildMinimal(2, 4, next), o[i]); } }
    // t=2, height 3, every node full: 63 keys
    { auto o = orders(63, 2);
      for (int i = 0; i < 3; i++) { int next = 1;
        removeAllTest(std::string("t=2 full tree, 63 keys, ") + names[i], 2, buildFull(2, 3, next), o[i]); } }
    // t=3, height 3, every node minimal: 26 keys
    { auto o = orders(26, 3);
      for (int i = 0; i < 3; i++) { int next = 1;
        removeAllTest(std::string("t=3 minimal tree, 26 keys, ") + names[i], 3, buildMinimal(3, 3, next), o[i]); } }
    // t=3, height 3, every node full: 215 keys
    { auto o = orders(215, 4);
      for (int i = 0; i < 3; i++) { int next = 1;
        removeAllTest(std::string("t=3 full tree, 215 keys, ") + names[i], 3, buildFull(3, 3, next), o[i]); } }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
