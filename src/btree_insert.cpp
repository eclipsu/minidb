// ============================================================================
// Hushay: search, insert (with splitting), update, in-order traversal, cleanup.
// Tests:  make test-insert
//
// Study first: CLRS chapter 18.1-18.2, and the USFCA B-Tree visualizer
//   https://www.cs.usfca.edu/~galles/visualization/BTree.html
//
// Use compareKeys(a, b) to compare keys (it returns <0, 0, >0).
// findIndex(node, key) is already written: the first index i with keys[i] >= key.
// ============================================================================
#include "btree.h"

// Look for `key`, starting at the root.
// At each node: i = findIndex(node, key).
//   If i < keys.size() and keys[i] equals key -> found: return &node->rows[i].
//   Else if the node is a leaf               -> not in the tree: return nullptr.
//   Else                                     -> go down into children[i] and repeat.
const Row* BTree::find(const Value& key) const {
    // TODO (Hushay)
    (void)key;
    return nullptr;
}

// 1. If the key already exists (use find), return false.
// 2. If the tree is empty: make a new leaf root holding just this key and row.
// 3. Else if the root is full (2t - 1 keys):
//        make a new root, put the old root as its children[0],
//        splitChild(newRoot, 0), then insert into the correct child.
//    This is the ONLY way the tree gets taller.
// 4. Call insertNonFull(root, key, row).
// 5. count++ and return true.
bool BTree::insert(const Value& key, const Row& row) {
    // TODO (Hushay)
    (void)key;
    (void)row;
    return false;
}

// parent->children[i] is full (2t - 1 keys). Split it into two nodes of t - 1 keys:
//   - the middle key (index t - 1) and its row move UP into parent at position i
//   - the keys/rows after the middle move into a NEW right node
//   - if the full node is not a leaf, its last t children move to the new node too
//   - the new node goes into parent->children at position i + 1
void BTree::splitChild(BTreeNode* parent, size_t i) {
    // TODO (Hushay)
    (void)parent;
    (void)i;
}

// `node` is guaranteed NOT full.
//   If it's a leaf: insert key and row at the sorted position (findIndex helps).
//   If not: find which child to go into (i = findIndex(node, key)).
//           If that child is full: splitChild(node, i), then decide whether the
//           key now goes into children[i] or children[i + 1].
//           Recurse: insertNonFull(child, key, row).
void BTree::insertNonFull(BTreeNode* node, const Value& key, const Row& row) {
    // TODO (Hushay)
    (void)node;
    (void)key;
    (void)row;
}

// Find the key and replace its row. Return false if the key isn't there.
// Hint: the same walk as find(), but you need a non-const pointer to change the row.
bool BTree::update(const Value& key, const Row& newRow) {
    // TODO (Hushay)
    (void)key;
    (void)newRow;
    return false;
}

// Every (key, row) pair in ascending order.
// In-order walk of a B-Tree node with k keys:
//   child 0, key 0, child 1, key 1, ..., key k-1, child k
// (a leaf has no children: just its keys in order)
std::vector<std::pair<Value, Row>> BTree::traverseInOrder() const {
    std::vector<std::pair<Value, Row>> out;
    // TODO (Hushay): a small recursive helper function makes this easy
    return out;
}

// Free `node` and everything below it (children first, then the node itself).
// Must handle node == nullptr. Called by the destructor.
void BTree::destroy(BTreeNode* node) {
    // TODO (Hushay)
    (void)node;
}
