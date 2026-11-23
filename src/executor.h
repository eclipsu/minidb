#pragma once

#include "btree.h"
#include "statement.h"
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct Table {
    std::vector<Column> columns;   // columns[0] is the key column
    BTree rows;                    // key = row[0]
};

struct Result {
    bool ok = false;
    std::string message;               // "1 row inserted", or the error when ok is false
    std::vector<std::string> header;   // SELECT only: the column names
    std::vector<Row> rows;             // SELECT only: the rows, already cut down to `header`
    size_t affected = 0;               // rows inserted / updated / deleted / truncated / returned
                                       // (0 for CREATE and DROP)
};

class Database {
public:
    Result execute(const Statement& stmt);

    bool hasTable(const std::string& name) const;
    const Table* getTable(const std::string& name) const;   // nullptr if there's no such table
    std::vector<std::string> tableNames() const;            // sorted

    // true after any statement that changed something,
    // false again after saveAll() or loadAll() succeeds.
    bool hasUnsavedChanges() const;

    // Writes every table to <dir>/<table>.csv with saveTable() from storage.h.
    // Creates `dir` if it doesn't exist, and deletes .csv files in it for
    // tables that no longer exist (so a dropped table stays dropped).
    Result saveAll(const std::string& dir);

    // Replaces ALL current tables with the ones in <dir>/*.csv (loadTable()).
    // If any file fails to load, nothing changes.
    Result loadAll(const std::string& dir);

private:
    // Tables are behind unique_ptr because a BTree can't be copied or moved.
    std::map<std::string, std::unique_ptr<Table>> tables;
    bool dirty = false;

    Table* findTable(const std::string& name);

    Result createTable(const Statement& stmt);
    Result insertRow(const Statement& stmt);
    Result selectRows(const Statement& stmt) const;
    Result updateRows(const Statement& stmt);
    Result deleteRows(const Statement& stmt);
    Result dropTable(const Statement& stmt);
    Result truncateTable(const Statement& stmt);
};

// Does `row` pass the WHERE clause?
//   where == nullptr        -> true (no WHERE means every row)
//   COMPARE                 -> row's value in where->column  <op>  where->value
//                              INT as numbers (9 < 10), TEXT alphabetically
//   AND / OR                -> combine matches() of where->left and where->right
// A column that isn't in `columns` never matches.
bool matches(const Expr* where, const Row& row, const std::vector<Column>& columns);

// Turns a Result into text for the REPL:
//   error   -> "Error: <message>"
//   SELECT  -> a table with lined up columns and a row count
//   other   -> the message
std::string formatResult(const Result& r);
