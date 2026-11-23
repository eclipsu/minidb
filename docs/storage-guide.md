# Storage Guide (Dennison)

This guide explains how to build save/load for MiniDB: writing a table to a CSV file and reading it back. You have **your own file** and **your own tests**. When all your tests say `PASS`, open a pull request.

| Who          | Your file           | Your tests                          | Run                 |
| ------------ | ------------------- | ----------------------------------- | ------------------- |
| **Dennison** | `src/storage.cpp`   | `tests/storage_test.cpp` (71 tests) | `make test-storage` |

The starter already passes 27 of them. Those are the "return false" tests (bad files, missing files) that a stub passes by accident. They'll keep you honest once you start returning true.

---

## 1. Setup (once)

```bash
git clone <repo-url>
cd minidb
git checkout storage-starter
make test-storage        # should say 27 passed, 44 failed
```

You need `g++` and `make`. On Windows, use WSL. On Mac, run `xcode-select --install`.

---

## 2. Daily workflow

```bash
git checkout storage-starter
git pull

git checkout -b storage           # your branch
# ...write code...
make test-storage

git add src/storage.cpp
git commit -m "Implement parseCsv"
git push -u origin storage        # first push; after that just: git push
```

When **all 71 tests pass**, open a **Pull Request** from `storage` into `storage-starter` (not `main`). GitHub runs the tests and shows a check for each part. Only the `storage` check is yours. Ask someone to look at it, then merge.

Push often, it's a free backup. Just don't **merge** until `storage` is green.

---

## 3. Rules

1. **Don't change `src/storage.h`** without telling the group. The tests depend on those exact names.
2. **Don't edit the B-Tree files.** Use the tree through its public functions (`insert`, `find`, `traverseInOrder`, `size`).
3. **Don't edit the test file** to make it pass. If you think a test is wrong, ask in the group chat.
4. **Only edit `src/storage.cpp`.**

---

## 4. The file format

A table with columns `id INT, name TEXT, age INT` is saved like this:

```
id INT,name TEXT,age INT
1,alice,20
2,bob,25
```

- **Line 1 is the header:** each column as `name TYPE`, joined with commas. TYPE is exactly `INT` or `TEXT`.
- **Every other line is one row**, in key order (that's what `traverseInOrder()` gives you).
- **The key is always the first column.**

The tricky part is text with commas, quotes or newlines in it. A field gets wrapped in quotes **only** when it needs it, and a quote inside becomes two quotes:

| Value          | In the file          |
| -------------- | -------------------- |
| `bob`          | `bob`                |
| `a,b`          | `"a,b"`              |
| `say "hi"`     | `"say ""hi"""`       |
| `two` + newline + `lines` | `"two` + newline + `lines"` |
| (empty)        | (nothing)            |

So one row can take up more than one line in the file. That's why you can't just split the file on `\n`, you have to go character by character (`parseCsv`).

---

## 5. Do the functions in this order

The tests are in the same order. The comments in `src/storage.cpp` walk through each one step by step.

| Step | Function         | Test section | Notes |
| ---- | ---------------- | ------------ | ----- |
| 1    | `escapeCsvField` | 1            | A few lines. Use it in `saveTable`. |
| 2    | `parseCsv`       | 2            | The hardest one. Keep an `inQuotes` flag. |
| 3    | `saveTable`      | 3            | Write `<path>.tmp`, then `std::rename` it over `path`. |
| 4    | `loadTable`      | 4, 5         | Check everything first, change things last. |
| -    | all of them      | 6            | Save then load, should get the same table back. |

**Key ideas:**

- **`parseCsv`:** inside quotes, a comma or newline is just part of the field. A `"` followed by another `"` is one quote character. A `"` on its own ends the quoted part.
- **`"a\n"` is ONE record**, not two. Don't add an empty record after the last newline.
- **Why `.tmp` then rename?** If the program crashes halfway through writing, you'd lose the old file. Writing to `.tmp` first means the real file is only replaced once the new one is complete. There's a test that blocks the `.tmp` file on purpose and checks the old file is untouched.
- **`loadTable` must be all or nothing.** Read everything into local variables, run every check, and only at the very end touch `columns` and `tree`. If row 900 is broken, rows 1 to 899 must not end up in the tree.
- **Bad INT values:** `-5` and `42` are fine. `abc`, `2.5`, `-`, empty, `" 5"` (with a space) are not. A number too big for a `long long` also has to be rejected, otherwise `std::stoll` throws when the tree compares keys and the program crashes.
- **Duplicate keys:** easiest way is to insert every row into a temporary `BTree` first. `insert` returns false on a duplicate. Note `9` and `09` count as the same INT key.

---

## 6. When a test fails

Each failure says what it expected and what it got. Special characters are shown escaped so you can see them:

```
  FAIL  escape "say \"hi\""
        expected "\"say \"\"hi\"\"\"" but got "say \"hi\""
```

**"loadTable returned true for ..."** in section 5 means you accepted a broken file. **"columns changed" / "tree changed"** means you returned false but changed things first.

**If you see `CRASH`**, the crashed test is the one right **after** the last PASS/FAIL line. Usually it's reading past the end of a string (`content[i + 1]` on the last character) or `std::stoll` throwing. To find the exact line:

```bash
make test-asan T=test-storage
```

The test files go in `storage_test_tmp/`. If a test crashes, that folder might be left behind. `make clean` deletes it.

---

## 7. Resources

- **RFC 4180**: the (short) CSV spec, sections 2.5 to 2.7 cover quoting: https://www.rfc-editor.org/rfc/rfc4180
- **cppreference**: `std::ifstream` / `std::ofstream`, `std::rename`, `std::remove`, `std::stoll`
- Read a whole file into a string:
  ```cpp
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  std::string content = ss.str();
  ```
  Open files with `std::ios::binary` so `\r\n` isn't changed on Windows.

---

## 8. Done checklist

- [ ] `make test-storage` shows **71 passed, 0 failed**
- [ ] `make test-asan T=test-storage` shows no errors
- [ ] `make test` still passes for everything else
- [ ] Pull request opened, `storage` check is green ✅
