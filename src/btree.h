#pragma once
// ============================================================================
// B-Tree: the primary data structure of MiniDB.
//
// Each table stores its rows in one BTree, keyed by the table's first column.
//   key = the first column's value (e.g. id 5)
//   row = all of the row's values  (e.g. {5, 'bob', 20})
//
// DO NOT change this file without telling the group: the tests and everyone's
// code depend on these exact names.
//
// Who writes what:
//   btree.cpp         shared helpers, ALREADY DONE (compareKeys, checkValid, ...)
//   btree_insert.cpp  Hushay: find, insert, splitChild, insertNonFull,
//                             update, traverseInOrder, destroy
//   btree_remove.cpp  Z:      remove and its helpers (borrow / merge)
//
// Rules (CLRS chapter 18 style):
//   * t = minimum degree (t >= 2). Every node holds at most 2t - 1 keys.
//     Every node except the root holds at least t - 1 keys.
//   * An empty tree has root == nullptr.
//     When the last key is removed, delete the root and set it to nullptr.
//   * Keys inside a node are sorted (use compareKeys, never < on the text).
//   * rows[i] always belongs to keys[i].
//   * An internal node with k keys has exactly k + 1 children.
//   * All leaves are at the same depth.
// ============================================================================

#include "statement.h"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// One row of a table: all of its values in column order.
using Row = std::vector<Value>;

// Compares two keys. Returns < 0 if a < b, 0 if equal, > 0 if a > b.
// INT keys compare as numbers (so 9 < 10), TEXT keys alphabetically.
// (Already written in btree.cpp.)
int compareKeys(const Value& a, const Value& b);

struct BTreeNode {
    bool leaf = true;
    std::vector<Value> keys;            // sorted keys
    std::vector<Row> rows;              // rows[i] belongs to keys[i]
    std::vector<BTreeNode*> children;   // empty for a leaf, keys.size() + 1 otherwise
};

class BTree {
public:
    explicit BTree(int t = 3);          // t = minimum degree
    ~BTree();                           // frees every node (calls destroy)

    BTree(const BTree&) = delete;             // no copying (it owns raw pointers)
    BTree& operator=(const BTree&) = delete;

    // ---------- Hushay (btree_insert.cpp) ----------

    // Adds a key and its row. Returns false (and changes nothing) if the key already exists.
    bool insert(const Value& key, const Row& row);

    // Returns a pointer to the key's row, or nullptr if the key is not in the tree.
    const Row* find(const Value& key) const;

    // Replaces the row stored under `key`. Returns false if the key is not in the tree.
    bool update(const Value& key, const Row& newRow);

    // Every (key, row) pair in ascending key order.
    std::vector<std::pair<Value, Row>> traverseInOrder() const;

    // ---------- Z (btree_remove.cpp) ----------

    // Removes the key and its row. Returns false if the key is not in the tree.
    bool remove(const Value& key);

    // ---------- Already done (btree.cpp) ----------

    size_t size() const;                // number of keys stored
    int height() const;                 // 0 for an empty tree, 1 for a single leaf
    int minDegree() const;              // t
    const BTreeNode* getRoot() const;   // for printing / tests

    // Checks every B-Tree rule. Returns true if the tree is valid; otherwise
    // false, and writes the reason into *why (if why is not nullptr).
    bool checkValid(std::string* why = nullptr) const;

    // TEST ONLY: replace the whole tree with one built by hand.
    // The tree takes ownership of newRoot (and frees the old tree).
    void loadTreeForTesting(BTreeNode* newRoot);

private:
    BTreeNode* root = nullptr;
    int t;
    size_t count = 0;

    // Index of the first key in `node` that is >= key (keys.size() if none).
    // This is "search within a node". (Already done in btree.cpp.)
    static size_t findIndex(const BTreeNode* node, const Value& key);

    // ---------- Hushay's helpers (btree_insert.cpp) ----------
    void splitChild(BTreeNode* parent, size_t i);                      // split parent->children[i]
    void insertNonFull(BTreeNode* node, const Value& key, const Row& row);
    static void destroy(BTreeNode* node);                              // free node and everything under it

    // ---------- Z's helpers (btree_remove.cpp) ----------
    // You don't HAVE to use these, but they follow CLRS / GeeksforGeeks.
    void removeFromNode(BTreeNode* node, const Value& key);
    void removeFromLeaf(BTreeNode* node, size_t idx);
    void removeFromInternal(BTreeNode* node, size_t idx);
    std::pair<Value, Row> getPredecessor(BTreeNode* node, size_t idx);
    std::pair<Value, Row> getSuccessor(BTreeNode* node, size_t idx);
    void fill(BTreeNode* node, size_t idx);            // make children[idx] have at least t keys
    void borrowFromPrev(BTreeNode* node, size_t idx);
    void borrowFromNext(BTreeNode* node, size_t idx);
    void merge(BTreeNode* node, size_t idx);           // merge children[idx] and children[idx + 1]
};
