# MiniDB

A small SQL database written from scratch in C++ for our Data Structures & Algorithms class. No external libraries or database engines, everything is hand written.

You type SQL into a prompt, MiniDB tokenizes it, parses it, and (once the executor is done) runs it against tables stored in B-Trees.

## Where we're at

```
Tokenizer  ->  Parser  ->  Validator  ->  Executor  ->  Storage (B-Tree)
  done         done        not yet       not yet       B-Tree done
```

- **Tokenizer** handles every keyword, symbol, number and string we need.
- **Parser** handles `CREATE TABLE`, `INSERT`, `SELECT` (with `WHERE` and `ORDER BY`), `UPDATE`, `DELETE`, `DROP TABLE` and `TRUNCATE TABLE`.
- **B-Tree** has insert, find, update, remove and in-order traversal working.
- **Validator / executor** aren't started yet, so right now the REPL just prints what it parsed.

There are 283 tests and they all pass. CI runs `make test` on every push and PR.

## Supported SQL

```sql
CREATE TABLE users (id INT, name TEXT, age INT);
INSERT INTO users VALUES (1, 'bob', 20);
SELECT * FROM users;
SELECT name, age FROM users WHERE age > 18 AND (name = 'bob' OR id = 2) ORDER BY age DESC;
UPDATE users SET age = 21 WHERE id = 1;
DELETE FROM users WHERE id = 1;
DROP TABLE users;
TRUNCATE TABLE users;
```

`WHERE` works with `=`, `!=`, `<`, `>`, `<=`, `>=`, `AND`, `OR` and parentheses. Types are just `INT` and `TEXT`.

## Building

You need g++ (or clang++) with C++17 and `make`.

```bash
make            # builds ./minidb
./minidb
make test       # runs every test
make clean
```

You can also run one group of tests at a time:

```bash
make test-parser    # tokenizer + parser
make test-insert    # B-Tree insert/find/update/traverse
make test-remove    # B-Tree remove
make test-full      # random inserts/removes compared against std::map
make test-asan      # everything with AddressSanitizer
```

## Example

```
db > CREATE TABLE users (id INT, name TEXT, age INT);
Parsed CREATE TABLE users with columns: id(INT) name(TEXT) age(INT)
db > INSERT INTO users VALUES (1, 'bob', 20);
Parsed INSERT INTO users with values: 1 'bob' 20
db > SELECT name FROM users WHERE age > 18 ORDER BY age DESC;
Parsed SELECT from users, columns: name, WHERE (age > 18), ORDER BY age(DESC)
db > DELETE FROM users WHERE id = 1;
Parsed DELETE from users, WHERE (id = 1)
db > SELECT FROM users;
Syntax error: Expected column names but found 'FROM'
db > .exit
```

Up/down arrows go through your command history. `.exit` or Ctrl-D quits.

## How it works

1. The **tokenizer** splits the input into tokens, e.g. `SELECT`, `users`, `>`, `18`.
2. The **parser** is recursive descent. It checks the grammar and builds a `Statement`, or throws a `ParseError`. `WHERE` clauses become an expression tree (`AND`/`OR` nodes with comparisons at the leaves).
3. The **validator** (todo) will check that tables and columns actually exist.
4. The **executor** (todo) will run the statement.
5. **Storage**: each table keeps its rows in a B-Tree keyed by its first column.

For example `CREATE TABLE users (name TEXT, age INT);` tokenizes to

```
CREATE  TABLE  IDENTIFIER(users)  (  IDENTIFIER(name)  TEXT  ,  IDENTIFIER(age)  INT  )  ;  END
```

and parses to

```
       CREATE
      /      \
   users   columns
           /     \
      name TEXT  age INT
```

### B-Tree

We followed CLRS chapter 18. With minimum degree `t`, a node holds at most `2t - 1` keys and every node except the root holds at least `t - 1`. Insert splits full nodes on the way down, and remove borrows from a sibling or merges on the way down, so neither one has to go back up the tree. find, insert, update and remove are all O(log n).

Keys are compared with `compareKeys`, so INT keys sort as numbers (9 before 10) and TEXT sorts alphabetically. The tests run `checkValid()` after each operation to make sure the tree is still a valid B-Tree.

## Files

```
src/
  main.cpp           REPL
  tokenizer.h/.cpp   string -> tokens
  parser.h/.cpp      tokens -> Statement
  statement.h        Statement, Column, Value, Expr
  btree.h            B-Tree interface
  btree.cpp          compareKeys, checkValid, getters
  btree_insert.cpp   find, insert, split, update, traverse
  btree_remove.cpp   remove, borrow, merge
tests/               one test file per part
GUIDE.MD             how we split up the B-Tree work
```

## TODO

- [ ] Table storage on top of the B-Tree
- [ ] Validator
- [ ] Execute `CREATE TABLE` and `INSERT`
- [ ] Execute `SELECT` with `WHERE` and `ORDER BY`
- [ ] Execute `UPDATE`, `DELETE`, `DROP`, `TRUNCATE`
- [ ] Maybe: save/load tables to a file

## Team

- [Rajeev Shrestha](https://github.com/eclipsu): tokenizer, parser, REPL, database engine
- [Hushay Madzingira](https://github.com/hushaymadzingira): B-Tree insert, find, update, traversal
- [Zander Dennison](https://github.com/ZanderSD): B-Tree remove

## References

- [Crafting Interpreters](https://craftinginterpreters.com/) (scanning and parsing chapters)
- CLRS, _Introduction to Algorithms_, chapter 18
- [USFCA B-Tree visualizer](https://www.cs.usfca.edu/~galles/visualization/BTree.html)
