// ============================================================================
// Z: removing a key, keeping the tree balanced with borrow and merge.
// Tests:  make test-remove
//
// Your tests build trees BY HAND, so you can start before insert is finished.
//
// Study first: CLRS chapter 18.3, the GeeksforGeeks article "Delete Operation
// in B-Tree", and the USFCA B-Tree visualizer (try deleting keys and watch).
//
// The idea (CLRS): never step down into a child that has only t - 1 keys.
// Before going down, fill() that child so it has at least t keys. Then a key
// can always be removed from a leaf without breaking the minimum.
//
// Use compareKeys(a, b) to compare keys. findIndex(node, key) is already written.
// ============================================================================
#include "btree.h"

// 1. If the tree is empty (root == nullptr), return false.
//    If the key isn't in the tree, return false. Don't use find() for this:
//    it's Hushay's, and your tests must pass even before hers do. Write a
//    small walk of your own (same idea as find: findIndex, then go down).
// 2. removeFromNode(root, key).
// 3. If the root now has 0 keys:
//        if it's a leaf -> the tree is empty: delete root, root = nullptr
//        else           -> the tree shrinks: root = its only child, delete the old root
//    This is the ONLY way the tree gets shorter.
// 4. count-- and return true.
bool BTree::remove(const Value& key) {
    // TODO (Z)
    (void)key;
    return false;
}

// Remove `key` from the subtree rooted at `node`.
//   idx = findIndex(node, key)
//   Case A: key is in this node (idx < keys.size() and keys[idx] == key)
//             leaf     -> removeFromLeaf(node, idx)
//             internal -> removeFromInternal(node, idx)
//   Case B: key is not in this node
//             leaf     -> key isn't in the tree (stop)
//             internal -> if children[idx] has only t - 1 keys, fill(node, idx) first.
//                         (fill may merge, so recompute which child to go into!)
//                         then recurse into that child.
void BTree::removeFromNode(BTreeNode* node, const Value& key) {
    // TODO (Z)
    (void)node;
    (void)key;
}

// Simple: erase keys[idx] and rows[idx].
void BTree::removeFromLeaf(BTreeNode* node, size_t idx) {
    // TODO (Z)
    (void)node;
    (void)idx;
}

// keys[idx] is in an internal node. Three cases (CLRS 2a, 2b, 2c):
//   2a. children[idx] has >= t keys:
//         replace keys[idx]/rows[idx] with the PREDECESSOR, then remove the
//         predecessor from children[idx].
//   2b. else if children[idx + 1] has >= t keys:
//         same thing with the SUCCESSOR and children[idx + 1].
//   2c. else both have t - 1 keys:
//         merge(node, idx), then remove the key from the merged child.
void BTree::removeFromInternal(BTreeNode* node, size_t idx) {
    // TODO (Z)
    (void)node;
    (void)idx;
}

// The largest key in the left subtree: start at children[idx], keep going to
// the LAST child until you reach a leaf, return its last key and row.
std::pair<Value, Row> BTree::getPredecessor(BTreeNode* node, size_t idx) {
    // TODO (Z)
    return {node->keys[idx], node->rows[idx]};
}

// The smallest key in the right subtree: start at children[idx + 1], keep going
// to the FIRST child until you reach a leaf, return its first key and row.
std::pair<Value, Row> BTree::getSuccessor(BTreeNode* node, size_t idx) {
    // TODO (Z)
    return {node->keys[idx], node->rows[idx]};
}

// children[idx] has only t - 1 keys. Make it have at least t:
//   if the LEFT sibling (idx - 1) has >= t keys  -> borrowFromPrev(node, idx)
//   else if the RIGHT sibling (idx + 1) has >= t -> borrowFromNext(node, idx)
//   else merge with a sibling: merge(node, idx) if there is a right sibling,
//                              otherwise merge(node, idx - 1)
void BTree::fill(BTreeNode* node, size_t idx) {
    // TODO (Z)
    (void)node;
    (void)idx;
}

// Rotate right through the parent:
//   parent's keys[idx - 1] moves DOWN to the front of children[idx]
//   left sibling's LAST key moves UP into parent's keys[idx - 1]
//   if not leaves: left sibling's last child moves to the front of children[idx]
// (Move the rows together with their keys!)
void BTree::borrowFromPrev(BTreeNode* node, size_t idx) {
    // TODO (Z)
    (void)node;
    (void)idx;
}

// Rotate left through the parent:
//   parent's keys[idx] moves DOWN to the end of children[idx]
//   right sibling's FIRST key moves UP into parent's keys[idx]
//   if not leaves: right sibling's first child moves to the end of children[idx]
void BTree::borrowFromNext(BTreeNode* node, size_t idx) {
    // TODO (Z)
    (void)node;
    (void)idx;
}

// Merge children[idx + 1] INTO children[idx]:
//   left = children[idx], right = children[idx + 1]
//   parent's keys[idx] moves DOWN to the end of left
//   all of right's keys, rows (and children, if not leaves) go to the end of left
//   remove keys[idx], rows[idx] and children[idx + 1] from the parent
//   delete right (careful: don't destroy() it, its children now belong to left)
void BTree::merge(BTreeNode* node, size_t idx) {
    // TODO (Z)
    (void)node;
    (void)idx;
}
