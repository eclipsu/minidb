# MiniDB

A tiny SQL database built from scratch in C++ for our Data Structures & Algorithms class.

MiniDB reads SQL commands from the terminal, breaks them into tokens, parses them, and will run them against tables stored in memory. No external libraries, no existing database engines — everything is written by hand.

## Current Status

The front end works: MiniDB can tokenize any command and parse `CREATE TABLE` and `INSERT`. Nothing is stored or executed yet.

```
Tokenizer  →  Parser  →  Validator  →  Executor  →  Storage (B-Tree)
   ✅          🟡           ⬜            ⬜             ⬜
```

✅ done  🟡 in progress  ⬜ not started

| Stage     | Status                             | Tests            |
| --------- | ---------------------------------- | ---------------- |
| Tokenizer | All planned SQL tokens             | 46 passing       |
| Parser    | `CREATE TABLE` and `INSERT`        | 32 passing       |
| Validator | Not started                        | —                |
| Executor  | Not started                        | —                |
| Storage   | Not started                        | —                |

## Features

| Command        | Example                                                  | Status      |
| -------------- | -------------------------------------------------------- | ----------- |
| `CREATE TABLE` | `CREATE TABLE users (id INT, name TEXT, age INT);`       | 🟡 Parsed   |
| `INSERT`       | `INSERT INTO users VALUES (1, 'bob', 20);`               | 🟡 Parsed   |
| `SELECT`       | `SELECT * FROM users;` or `SELECT name, age FROM users;` | ⬜ Planned  |
| `WHERE`        | `SELECT * FROM users WHERE age > 18;`                    | ⬜ Planned  |
| `UPDATE`       | `UPDATE users SET age = 21 WHERE id = 1;`                | ⬜ Planned  |
| `DELETE`       | `DELETE FROM users WHERE id = 1;`                        | ⬜ Planned  |

Type `.exit` to quit.

## Build & Run

Requires a C++17 compiler (g++ or clang++) and `make`.

```bash
make        # build ./minidb
./minidb
make test   # build and run all tests
make clean  # remove built binaries
```

## Example Session

This is what works today. The REPL prints what it parsed, because there is no executor yet.

```
db > CREATE TABLE users (name TEXT, age INT);
Parsed CREATE TABLE users with columns: name(TEXT) age(INT)
db > CREATE TABLE users (id)
Syntax error: Expected Int or Text
db > INSERT INTO users VALUES (1, 'bob', 20);
Parsed INSERT INTO users with values: 1 'bob' 20
db > INSERT INTO users VALUES (id)
Syntax error: Expected a number or 'text'
db > SELECT * FROM users;
Syntax error: Unknown command
db > .exit
```

## How It Works

Every command goes through the same pipeline:

1. **Tokenizer** — splits the raw string into tokens (`SELECT`, `users`, `>`, `18`, ...).
2. **Parser** — checks the tokens follow SQL grammar and builds a `Statement`. Bad syntax throws a `ParseError`.
3. **Validator** — checks the statement makes sense, like whether the table and columns exist.
4. **Executor** — runs the statement against the stored tables.
5. **Storage** — keeps the rows, indexed with a B-tree.

### Example: `CREATE TABLE`

```sql
CREATE TABLE users (name TEXT, age INT);
```

The tokenizer turns the string into a flat list:

```
CREATE  TABLE  IDENTIFIER(users)  (  IDENTIFIER(name)  TEXT  ,  IDENTIFIER(age)  INT  )  ;  END
```

The parser turns that list into a `Statement`:

```
       CREATE
      /      \
   users   columns
           /     \
      name TEXT  age INT
```

## Project Structure

```
minidb/
├── README.md
├── Makefile
├── src/
│   ├── main.cpp          # REPL loop: reads input, runs commands
│   ├── tokenizer.h/.cpp  # Turns a string into a list of tokens
│   ├── parser.h/.cpp     # Turns tokens into statement objects
│   └── statement.h       # Statement, Column, and value types
└── tests/
    ├── tokenizer_test.cpp
    └── parser_test.cpp
```

## Data Structures Used

- **Vector** — list of tokens, columns and values in a statement
- **Hash map** (`unordered_map`) _(planned)_ — table name → table lookup
- **Expression tree** _(planned)_ — WHERE clauses like `age > 18 AND name = 'bob'`
- **B-tree** _(planned)_ — index for fast lookups

## Roadmap

- [x] REPL loop with `.exit`
- [x] Tokenizer
- [x] Parse `CREATE TABLE`
- [x] Syntax error messages for `CREATE TABLE`
- [x] Parse `INSERT`
- [x] Syntax error messages for `INSERT`
- [ ] Validator
- [ ] Executor for `CREATE TABLE` and `INSERT`
- [ ] `SELECT *` and column selection
- [ ] `WHERE` with `=`, `<`, `>`, `!=`
- [ ] `WHERE` with `AND` / `OR`
- [ ] `UPDATE` and `DELETE`
- [ ] Storage with a B-tree index
- [ ] _Stretch:_ save/load tables to a file

## Team

- Rajeev Shrestha — Tokenizer, Parser, Database Engine

## References

- [Crafting Interpreters — Scanning](https://craftinginterpreters.com/scanning.html)
