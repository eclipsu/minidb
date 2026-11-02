// Shared B-Tree helpers. ALREADY DONE: you don't need to edit this file.
#include "btree.h"
#include <stdexcept>

// ---------- Comparing keys ----------

int compareKeys(const Value& a, const Value& b) {
    if (a.type == ValueType::INT && b.type == ValueType::INT) {
        long long x = std::stoll(a.text);
        long long y = std::stoll(b.text);
        if (x < y) return -1;
        if (x > y) return 1;
        return 0;
    }
    if (a.type == ValueType::TEXT && b.type == ValueType::TEXT) {
        return a.text.compare(b.text);
    }
    // Mixed types: every INT sorts before every TEXT.
    return a.type == ValueType::INT ? -1 : 1;
}

// ---------- Simple getters ----------

BTree::BTree(int t) : t(t) {
    if (t < 2) throw std::invalid_argument("B-Tree minimum degree t must be at least 2");
}

BTree::~BTree() {
    destroy(root);
}

size_t BTree::size() const { return count; }
int BTree::minDegree() const { return t; }
const BTreeNode* BTree::getRoot() const { return root; }

int BTree::height() const {
    int h = 0;
    for (const BTreeNode* n = root; n != nullptr; n = n->leaf ? nullptr : n->children[0]) h++;
    return h;
}

size_t BTree::findIndex(const BTreeNode* node, const Value& key) {
    size_t i = 0;
    while (i < node->keys.size() && compareKeys(node->keys[i], key) < 0) i++;
    return i;
}

// ---------- Test helpers ----------

static size_t countKeys(const BTreeNode* node) {
    if (node == nullptr) return 0;
    size_t total = node->keys.size();
    for (const BTreeNode* c : node->children) total += countKeys(c);
    return total;
}

void BTree::loadTreeForTesting(BTreeNode* newRoot) {
    destroy(root);
    root = newRoot;
    count = countKeys(newRoot);
}

// Walks the tree and checks every B-Tree rule.
// lo / hi: every key in this subtree must be strictly between them (nullptr = no limit).
static bool checkNode(const BTreeNode* node, int t, bool isRoot, int depth, int& leafDepth,
                      const Value* lo, const Value* hi, std::string& why) {
    const size_t n = node->keys.size();
    const size_t maxKeys = 2 * t - 1;
    const size_t minKeys = isRoot ? 1 : t - 1;

    if (n > maxKeys) { why = "a node has " + std::to_string(n) + " keys (max is " + std::to_string(maxKeys) + ")"; return false; }
    if (n < minKeys) { why = std::string(isRoot ? "the root" : "a non-root node") + " has " + std::to_string(n) +
                             " keys (min is " + std::to_string(minKeys) + ")"; return false; }
    if (node->rows.size() != n) { why = "a node's rows.size() does not match keys.size()"; return false; }

    for (size_t i = 0; i < n; i++) {
        if (i > 0 && compareKeys(node->keys[i - 1], node->keys[i]) >= 0) { why = "keys inside a node are not sorted (or repeat)"; return false; }
        if (lo && compareKeys(node->keys[i], *lo) <= 0) { why = "key " + node->keys[i].text + " is on the wrong side of its parent"; return false; }
        if (hi && compareKeys(node->keys[i], *hi) >= 0) { why = "key " + node->keys[i].text + " is on the wrong side of its parent"; return false; }
    }

    if (node->leaf) {
        if (!node->children.empty()) { why = "a leaf has children"; return false; }
        if (leafDepth == -1) leafDepth = depth;
        else if (leafDepth != depth) { why = "leaves are not all at the same depth"; return false; }
        return true;
    }

    if (node->children.size() != n + 1) {
        why = "an internal node with " + std::to_string(n) + " keys has " + std::to_string(node->children.size()) +
              " children (should be " + std::to_string(n + 1) + ")";
        return false;
    }
    for (size_t i = 0; i <= n; i++) {
        if (node->children[i] == nullptr) { why = "an internal node has a null child"; return false; }
        const Value* childLo = (i == 0) ? lo : &node->keys[i - 1];
        const Value* childHi = (i == n) ? hi : &node->keys[i];
        if (!checkNode(node->children[i], t, false, depth + 1, leafDepth, childLo, childHi, why)) return false;
    }
    return true;
}

bool BTree::checkValid(std::string* why) const {
    std::string reason;
    bool ok = true;
    if (root == nullptr) {
        if (count != 0) { reason = "root is nullptr but size() is " + std::to_string(count); ok = false; }
    } else {
        int leafDepth = -1;
        ok = checkNode(root, t, true, 0, leafDepth, nullptr, nullptr, reason);
        if (ok && countKeys(root) != count) {
            reason = "size() says " + std::to_string(count) + " but the tree holds " + std::to_string(countKeys(root)) + " keys";
            ok = false;
        }
    }
    if (!ok && why) *why = reason;
    return ok;
}
