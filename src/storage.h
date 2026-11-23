#pragma once
// Saving and loading a table to a CSV file.
//
// File format:
//   first line = the columns, as "name TYPE"   e.g.  id INT,name TEXT,age INT
//   then one line per row, in key order        e.g.  1,bob,20
//
// A field is wrapped in quotes only when it has a comma, a quote or a newline
// in it, and a quote inside a quoted field is written twice:
//   hi, "bob"   ->   "hi, ""bob"""
//
// DO NOT change this file without telling the group: the tests depend on
// these exact names.

#include "btree.h"
#include "statement.h"
#include <string>
#include <vector>

// Returns the field ready to go into a CSV line.
// Quote it only if it contains , " \n or \r. Every " inside becomes "".
std::string escapeCsvField(const std::string& field);

// Splits CSV text into records (lines), each a list of fields.
// `records` is cleared first.
// Handles quoted fields, "" inside quotes, newlines inside quotes, and both
// \n and \r\n line endings. The last line doesn't need a newline at the end.
// Empty content gives zero records.
// Returns false if a quote is never closed.
bool parseCsv(const std::string& content, std::vector<std::vector<std::string>>& records);

// Writes the table to `path`: the header line, then every row from
// tree.traverseInOrder().
// Write to "<path>.tmp" first, then rename it over `path`, so a failed save
// never leaves a half written file behind.
// Returns false if anything fails. The old file at `path` must be untouched.
bool saveTable(const std::string& path, const std::vector<Column>& columns, const BTree& tree);

// Reads a file written by saveTable into `columns` and `tree`.
// `tree` must be empty when you call this (return false if it isn't).
// Checks the WHOLE file before changing anything:
//   - quotes are all closed
//   - there is a header line, every column is "name INT" or "name TEXT"
//   - every row has the same number of fields as the header
//   - every INT value is a whole number that fits in a long long (-5, 42)
//   - no two rows have the same key (first column)
// Returns false if any check fails, and then `columns` and `tree` are unchanged.
bool loadTable(const std::string& path, std::vector<Column>& columns, BTree& tree);
