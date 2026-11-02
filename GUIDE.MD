# B-Tree Guide (Hushay & Z)

This guide explains how to build MiniDB's B-Tree, our primary data structure. Each of you has **your own file** and **your own tests**. When all your tests say `PASS`, open a pull request.

| Who                  | Your file              | Your tests                               | Run                |
| -------------------- | ---------------------- | ---------------------------------------- | ------------------ |
| **Hushay**           | `src/btree_insert.cpp` | `tests/btree_insert_test.cpp` (35 tests) | `make test-insert` |
| **Z**                | `src/btree_remove.cpp` | `tests/btree_remove_test.cpp` (32 tests) | `make test-remove` |
| **Both, at the end** |                        | `tests/btree_full_test.cpp` (5 tests)    | `make test-full`   |

You never edit the same file, so you won't get merge conflicts.

---

## 1. Setup (once)

```bash
git clone <repo-url>
cd minidb
git checkout btree-starter   # the B-Tree files live here, not on main
make test-parser             # should say all passed (the parser is already done)
```

You need `g++` and `make`. On Windows, use WSL. On Mac, run `xcode-select --install`.

---

## 2. Daily workflow

```bash
git checkout btree-starter
git pull                              # get the latest code

git checkout -b btree-insert          # Hushay  (Z: btree-remove)
# ...write code...
make test-insert                      # Z: make test-remove

git add src/btree_insert.cpp          # Z: src/btree_remove.cpp
git commit -m "Implement splitChild"
git push -u origin btree-insert       # first push; after that just: git push
```

When **all your tests pass**, go to GitHub and open a **Pull Request** from your branch into `btree-starter` (not `main`). GitHub runs the tests automatically and shows a green ✅ or red ❌. Ask someone to look at it, then merge.

You don't have to wait until everything passes to push. Pushing your branch often is a good backup. Just don't **merge** until it's green.

---

## 3. Rules

1. **Don't change `src/btree.h`** without telling the group. Everyone's code and all the tests depend on those exact names.
2. **Don't edit `src/btree.cpp`**. It has shared helpers that are already done.
3. **Don't edit the test files** to make them pass. If you think a test is wrong, ask in the group chat.
4. **Only edit your own file.**

---

## 4. How the B-Tree works here

- **`t` = minimum degree.** Every node has **at most `2t - 1` keys**, and every node except the root has **at least `t - 1` keys**. Tests use `t = 2` (max 3 keys per node), plus `t = 3`, `4` and `5`.
- **Empty tree:** `root == nullptr`.
- **Each node** (`BTreeNode`) has:
  - `keys`: sorted
  - `rows`: `rows[i]` belongs to `keys[i]`. **Whenever you move a key, move its row with it.**
  - `children`: empty for a leaf, otherwise exactly `keys.size() + 1`
  - `leaf`: `true` or `false`
- **Compare keys with `compareKeys(a, b)`**, never with `<` on the text. It returns `< 0`, `0` or `> 0`, and it knows that `9 < 10` for numbers. (Comparing the text would put `"10"` before `"9"`.)
- **`findIndex(node, key)`** is already written. It returns the first index `i` where `keys[i] >= key`. This is "search within a node," and you'll use it everywhere.
- **`count`** is the number of keys in the tree. Add 1 on a successful insert, subtract 1 on a successful remove.

---

## 5. Hushay: insert, find, update, traverse

Read **CLRS chapter 18.1–18.2** first, and play with the visualizer (section 8).

Do them **in this order**. The tests are in the same order:

| Step | Function                                                                             | Test section it unlocks                                                                   |
| ---- | ------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------- |
| 1    | `find`                                                                               | 1–2                                                                                       |
| 2    | `insert` + `insertNonFull` (leaf case only)                                          | 2–3                                                                                       |
| 3    | `traverseInOrder`                                                                    | 2, 8                                                                                      |
| 4    | `splitChild` + the full-root case in `insert` + the internal case in `insertNonFull` | 4–6                                                                                       |
| 5    | `update`                                                                             | 7                                                                                         |
| 6    | `destroy`                                                                            | no test fails without it, but it leaks memory (check with `make test-asan T=test-insert`) |

**Key ideas:**

- **Split before going down** (CLRS style). If the child you're about to enter is full, split it first. That guarantees there's always room in the leaf.
- **The tree only grows taller at the root.** When the root is full, make a new empty root, put the old root as its child, and split.
- **`splitChild(parent, i)`**: the middle key (index `t - 1`) moves **up** into the parent. The keys after it, their rows, and (if not a leaf) the last `t` children move to a **new right node**.
- **Duplicates:** if the key already exists, `insert` returns `false` and changes nothing.

---

## 6. Z: remove (borrow and merge)

Read **CLRS chapter 18.3** and the GeeksforGeeks article **"Delete Operation in B-Tree"** first, and try deleting keys in the visualizer (section 8).

**Your tests build trees by hand**, so you don't need Hushay's insert to start. Don't call `find()` or `insert()` in your code, because they may not work yet. Write your own small "does this key exist?" walk instead.

The comments in your file walk through every function. In short:

| Function                            | Job                                                                                                                                                        |
| ----------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `remove`                            | Return `false` if the key isn't there. Otherwise call `removeFromNode(root, key)`, then fix the root if it's empty (see below), then decrease `count`.     |
| `removeFromNode`                    | Key is here → leaf or internal case. Key not here → make sure the child you go into has at least `t` keys (`fill`), then go down.                          |
| `removeFromLeaf`                    | Just erase it.                                                                                                                                             |
| `removeFromInternal`                | CLRS 2a (predecessor), 2b (successor), 2c (merge).                                                                                                         |
| `fill`                              | Borrow from the left sibling, else the right sibling, else merge.                                                                                          |
| `borrowFromPrev` / `borrowFromNext` | Rotate a key through the parent. **Move a child too** if they're not leaves.                                                                               |
| `merge`                             | Parent key + right sibling get appended into the left child. Delete the right node (not with `destroy`, because its children now belong to the left node). |

**Key ideas:**

- **Never go down into a child with only `t - 1` keys.** Call `fill` first. Then removing from a leaf never breaks the minimum.
- **After `fill`, recheck where to go.** If you merged with the left sibling, the child you want moved one position to the left.
- **The tree only gets shorter at the root.** If the root ends up with 0 keys: if it's a leaf, the tree is empty (`delete root; root = nullptr;`); otherwise the root's only child becomes the new root.

---

## 7. When a test fails

Each failure tells you what it expected and what it got:

```
  FAIL  3a: borrow from the left sibling
        keys should be [3 5 10] but were [3 10]
```

**"tree is broken after remove: ..."** means `checkValid()` found a broken B-Tree rule, and the message says which one (too many keys, unsorted keys, leaves at different depths, a missing child...).

**If you see `CRASH`**, your code crashed (usually a bad pointer, or reading past the end of a `vector`). The crashed test is the one right **after** the last PASS/FAIL line. To find the exact line in your code:

```bash
make test-asan T=test-insert     # Hushay
make test-asan T=test-remove     # Z
```

Read the first few lines of the error: they point to your file and the line number.

**Debugging tip:** draw the tree on paper before and after the operation, then add prints:

```cpp
std::cout << "splitting child " << i << " which has " << child->keys.size() << " keys\n";
```

---

## 8. Resources

- **USFCA B-Tree visualizer**: https://www.cs.usfca.edu/~galles/visualization/BTree.html
  Set "Max. Degree" to **4**. That matches our `t = 2` (max 3 keys per node). Insert and delete numbers and watch the splits, borrows and merges.
- **CLRS (Introduction to Algorithms), chapter 18**: the algorithms our code follows
- **GeeksforGeeks**: "Insert Operation in B-Tree" and "Delete Operation in B-Tree"

---

## 9. Done checklist

**Hushay**

- [ ] `make test-insert` shows **35 passed, 0 failed**
- [ ] `make test-asan T=test-insert` shows no errors
- [ ] Pull request opened, `btree-insert` check is green ✅

**Z**

- [ ] `make test-remove` shows **32 passed, 0 failed**
- [ ] `make test-asan T=test-remove` shows no errors
- [ ] Pull request opened, `btree-remove` check is green ✅

**Together (after both are merged)**

- [ ] `make test-full` shows **5 passed, 0 failed**
- [ ] `make test` all green
- [ ] Pull request opened from `btree-starter` into `main`
