# MiniDB

A tiny SQL database built from scratch in C++ for our Data Structures & Algorithms class.

MiniDB reads SQL commands from the terminal, breaks them into tokens, parses them, and runs them against tables stored in memory. No external libraries, no existing database engines — everything is written by hand.

## Features

| Command        | Example                                                  |
| -------------- | -------------------------------------------------------- |
| `CREATE TABLE` | `CREATE TABLE users (id INT, name TEXT, age INT);`       |
| `INSERT`       | `INSERT INTO users VALUES (1, 'bob', 20);`               |
| `SELECT`       | `SELECT * FROM users;` or `SELECT name, age FROM users;` |
| `WHERE`        | `SELECT * FROM users WHERE age > 18;`                    |
| `UPDATE`       | `UPDATE users SET age = 21 WHERE id = 1;`                |
| `DELETE`       | `DELETE FROM users WHERE id = 1;`                        |

Type `.exit` to quit.

## Build & Run

Requires a C++17 compiler (g++ or clang++).

```bash
g++ -std=c++17 -Wall -o minidb src/*.cpp
./minidb
```

## Example Session

```
db > CREATE TABLE users (id INT, name TEXT, age INT);
Table 'users' created.
db > INSERT INTO users VALUES (1, 'bob', 20);
1 row inserted.
db > INSERT INTO users VALUES (2, 'alice', 17);
1 row inserted.
db > SELECT * FROM users WHERE age > 18;
id | name | age
1  | bob  | 20
db > .exit
```

## How It Works

Every command goes through the same pipeline:

```
input string → Tokenizer → Parser → Executor → Table
```

1. **Tokenizer** — splits the raw string into tokens (`SELECT`, `users`, `>`, `18`, ...).
2. **Parser** — checks the tokens follow SQL grammar and builds a statement object. WHERE conditions become an expression tree.
3. **Executor** — runs the statement: finds matching rows by evaluating the WHERE tree on each row.
4. **Table** — stores the rows and column definitions.

## Project Structure

```
minidb/
├── README.md
├── src/
│   ├── main.cpp          # REPL loop: reads input, runs commands
│   ├── tokenizer.h/.cpp  # Turns a string into a list of tokens
│   ├── parser.h/.cpp     # Turns tokens into statement objects
│   ├── statement.h       # Structs for each command + WHERE expression tree
│   ├── executor.h/.cpp   # Runs statements against tables
│   └── table.h/.cpp      # Table, Row, and Column storage
└── tests/
    └── sample.sql        # Example commands for testing
```

## Data Structures Used

- **Vector** — list of tokens, rows in each table
- **Hash map** (`unordered_map`) — table name → table lookup
- **Expression tree** — WHERE clauses like `age > 18 AND name = 'bob'`
- **B-tree** _(stretch goal)_ — index on the primary key for fast lookups

## Roadmap

- [ ] REPL loop with `.exit`
- [ ] Tokenizer
- [ ] `CREATE TABLE` and `INSERT`
- [ ] `SELECT *` and column selection
- [ ] `WHERE` with `=`, `<`, `>`, `!=`
- [ ] `WHERE` with `AND` / `OR`
- [ ] `UPDATE` and `DELETE`
- [ ] Error messages for bad input
- [ ] _Stretch:_ save/load tables to a file
- [ ] _Stretch:_ B-tree index

## Team

- Rajeev Shrestha — Tokenizer, Database Engine

## References

- [Crafting Interpreters — Scanning](https://craftinginterpreters.com/scanning.html)
