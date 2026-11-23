//   error("...")              -> a failed Result with that message
//   done("...", n)            -> a successful Result with affected = n
//   findColumn(columns, name) -> index of the column, or -1
//   compareKeys(a, b)         -> from btree.h, < 0 / 0 / > 0, INT as numbers
//   table->rows.insert / find / update / remove / traverseInOrder / size

#include "executor.h"
#include "storage.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

static Result error(const std::string& message) {
    Result r;
    r.ok = false;
    r.message = message;
    return r;
}

static Result done(const std::string& message, size_t affected) {
    Result r;
    r.ok = true;
    r.message = message;
    r.affected = affected;
    return r;
}

[[maybe_unused]] static int findColumn(const std::vector<Column>& columns, const std::string& name) {
    for (size_t i = 0; i < columns.size(); i++)
        if (columns[i].name == name) return static_cast<int>(i);
    return -1;
}

[[maybe_unused]] static const char* typeName(ValueType t) { return t == ValueType::INT ? "INT" : "TEXT"; }

Result Database::createTable(const Statement& stmt) {
    
    if (hasTable(stmt.table)) return error("table " + stmt.table + " already exists");
    
    for (size_t i = 0; i < stmt.columns.size(); i++)
        for (size_t j = i + 1; j < stmt.columns.size(); j++)
            if (stmt.columns[i].name == stmt.columns[j].name)
                return error("column " + stmt.columns[i].name + " appears twice");

    // 3. all good: make the table and put it in the map
    auto table = std::make_unique<Table>();
    table->columns = stmt.columns;
    tables[stmt.table] = std::move(table);

    return done("Table " + stmt.table + " created", 0);
}

// ---------- INSERT ----------

// 1. findTable(stmt.table). nullptr -> error "no such table".
// 2. stmt.values.size() must equal the number of columns -> else error.
// 3. Every value's type must match its column's type:
//      INSERT INTO users VALUES ('5', 'bob', 20)   <- '5' is TEXT, id is INT: error
//    Say which column is wrong in the message (typeName() helps).
// 4. The key is stmt.values[0]. If table->rows.find(key) isn't nullptr
//    -> error "duplicate key".
// 5. table->rows.insert(key, stmt.values);
//    return done("1 row inserted", 1);

Result Database::insertRow(const Statement& stmt) {
    Table* table = findTable(stmt.table);
    if (table == nullptr) return error("Could not find table: " + stmt.table);

    if (stmt.values.size() != table->columns.size())
        return error("Table " + stmt.table + " has " + std::to_string(table->columns.size()) +
                     " columns, but " + std::to_string(stmt.values.size()) + " values were given");

    for (size_t i = 0; i < stmt.values.size(); i++) {
        const Column& column = table->columns[i];
        const Value& value = stmt.values[i];
        if (value.type != column.type)
            return error("column " + column.name + " is " + typeName(column.type) +
                         ", but the value " + value.text + " is " + typeName(value.type));
    }

    const Value& key = stmt.values[0];
    if (table->rows.find(key) != nullptr) return error("duplicate key");

    table->rows.insert(key, stmt.values);
    return done("1 row inserted", 1);
}

// ---------- WHERE ----------

// The check for a WHERE clause, before running it. Returns "" if it's fine,
// otherwise the error message.
//   nullptr    -> ""  (no WHERE)
//   AND / OR   -> check left, then right. Return the first error you find.
//                 (Check BOTH sides: "id = 1 OR nosuch = 2" is an error.)
//   COMPARE    -> the column must exist (findColumn), and the value must be
//                 the same type as the column:
//                   age = 'old'   INT column, text value   -> error
//                   name = 5      TEXT column, number      -> error
[[maybe_unused]] static std::string checkWhere(const Expr* where, const std::vector<Column>& columns) {
    if (where == nullptr) return "";
    if (where->kind != ExprKind::COMPARE) {
        std::string err = checkWhere(where->left.get(), columns);
        return err.empty() ? checkWhere(where->right.get(), columns) : err;
    }

    int i = findColumn(columns, where->column);
    if (i < 0) return "no such column " + where->column;

    if (columns[i].type != where->value.type)
        return "column " + where->column + " is " + typeName(columns[i].type) +
               ", but it is compared with the " + typeName(where->value.type) +
               " value " + where->value.text;

    return "";
}

// If the WHERE pins down the key column with "=", return that key value.
//   id = 5                     -> 5
//   id = 5 AND age > 18        -> 5   (look up row 5, then check the rest with matches())
//   age > 18 / id > 5 / OR     -> nullptr (those really need to look at every row)
// The caller must still run matches() on the row it finds, so the other
// conditions in an AND are respected.
static const Value* keyLookup(const Expr* where, const std::vector<Column>& columns) {
    if (where == nullptr || columns.empty()) return nullptr;
    if (where->kind == ExprKind::AND) {
        const Value* key = keyLookup(where->left.get(), columns);
        return key ? key : keyLookup(where->right.get(), columns);
    }
    if (where->kind == ExprKind::COMPARE && where->op == TokenType::EQUAL &&
        where->column == columns[0].name && where->value.type == columns[0].type)
        return &where->value;
    return nullptr;   // OR, or a comparison that isn't "key = value"
}

// For a WHERE that names a key the table doesn't have. The proposal asks
// GET, UPDATE and DELETE to report a missing key instead of just "0 rows".
static std::string noRowWith(const Column& keyColumn, const Value& key) {
    std::string shown = key.type == ValueType::TEXT ? "'" + key.text + "'" : key.text;
    return "no row with " + keyColumn.name + " = " + shown;
}

// See executor.h.
//   nullptr  -> true
//   AND      -> matches(left) && matches(right)    (where->left.get())
//   OR       -> matches(left) || matches(right)
//   COMPARE:
//     1. i = findColumn(columns, where->column). -1 -> false.
//     2. If row[i].type != where->value.type -> false.
//     3. int c = compareKeys(row[i], where->value);
//     4. switch on where->op:
//          TokenType::EQUAL -> c == 0,   NOT_EQUAL -> c != 0,
//          LESS -> c < 0,   GREATER -> c > 0,
//          LESS_EQUAL -> c <= 0,   GREATER_EQUAL -> c >= 0
bool matches(const Expr* where, const Row& row, const std::vector<Column>& columns) {
    if (where == nullptr) return true;

    if (where->kind == ExprKind::AND)
        return matches(where->left.get(), row, columns) && matches(where->right.get(), row, columns);
    if (where->kind == ExprKind::OR)
        return matches(where->left.get(), row, columns) || matches(where->right.get(), row, columns);

    int i = findColumn(columns, where->column);
    if (i < 0) return false;
    if (row[i].type != where->value.type) return false;

    int c = compareKeys(row[i], where->value);
    switch (where->op) {
        case TokenType::EQUAL:         return c == 0;
        case TokenType::NOT_EQUAL:     return c != 0;
        case TokenType::LESS:          return c < 0;
        case TokenType::GREATER:       return c > 0;
        case TokenType::LESS_EQUAL:    return c <= 0;
        case TokenType::GREATER_EQUAL: return c >= 0;
        default:                       return false;
    }
}

// ---------- SELECT ----------

// This one is const, so use getTable() instead of findTable().
//
// 1. getTable(stmt.table). nullptr -> error.
// 2. The header:
//      stmt.selectColumns empty (SELECT *) -> every column name, in order
//      otherwise -> stmt.selectColumns. Each must exist, else error.
//    Keep the column index of each one, you need them in step 7.
// 3. checkWhere(stmt.where.get(), columns). Not "" -> error with that message.
// 4. Every ORDER BY column must exist -> else error.
// 5. Go through table->rows.traverseInOrder() and keep every FULL row where
//    matches(stmt.where.get(), row, columns) is true.
//    (traverseInOrder gives key order, so with no ORDER BY that's the order.)
// 6. ORDER BY (only if stmt.orderBy isn't empty): std::stable_sort the kept
//    rows with a comparator that goes through stmt.orderBy:
//      for each item:  c = compareKeys(a[idx], b[idx])
//                      if c != 0 -> return item.desc ? c > 0 : c < 0
//      return false   (all equal: stable_sort keeps them in key order)
//    Sort BEFORE step 7: you can ORDER BY a column you didn't SELECT.
// 7. Cut each row down to the header columns (in header order).
// 8. Fill in the Result: ok, header, rows, affected = rows.size(),
//    message like "3 rows".
Result Database::selectRows(const Statement& stmt) const {
    const Table* table = getTable(stmt.table);
    if (table == nullptr) return error("no such table " + stmt.table);
    const std::vector<Column>& columns = table->columns;

    std::vector<std::string> header = stmt.selectColumns;
    if (header.empty())
        for (const Column& column : columns) header.push_back(column.name);

    std::vector<int> pick;
    for (const std::string& name : header) {
        int i = findColumn(columns, name);
        if (i < 0) return error("no such column " + name);
        pick.push_back(i);
    }

    std::string bad = checkWhere(stmt.where.get(), columns);
    if (!bad.empty()) return error(bad);

    std::vector<std::pair<int, bool>> sortKeys;
    for (const auto& item : stmt.orderBy) {
        int i = findColumn(columns, item.column);
        if (i < 0) return error("no such column " + item.column);
        sortKeys.emplace_back(i, item.desc);
    }

    std::vector<Row> kept;
    if (const Value* key = keyLookup(stmt.where.get(), columns)) {
        const Row* row = table->rows.find(*key);
        if (row == nullptr) {
            Result r;
            r.ok = true;
            r.header = header;
            r.message = noRowWith(columns[0], *key);
            return r;
        }
        if (matches(stmt.where.get(), *row, columns)) kept.push_back(*row);
    } else {
        for (const auto& entry : table->rows.traverseInOrder())
            if (matches(stmt.where.get(), entry.second, columns))
                kept.push_back(entry.second);
    }

    if (!sortKeys.empty())
        std::stable_sort(kept.begin(), kept.end(), [&](const Row& a, const Row& b) {
            for (const auto& [idx, desc] : sortKeys) {
                int c = compareKeys(a[idx], b[idx]);
                if (c != 0) return desc ? c > 0 : c < 0;
            }
            return false;
        });

    std::vector<Row> out;
    out.reserve(kept.size());
    for (const Row& row : kept) {
        Row shaped;
        for (int idx : pick) shaped.push_back(row[idx]);
        out.push_back(std::move(shaped));
    }

    Result r;
    r.ok = true;
    r.header = header;
    r.rows = std::move(out);
    r.affected = r.rows.size();
    r.message = std::to_string(r.rows.size()) + (r.rows.size() == 1 ? " row" : " rows");
    return r;
}

// ---------- UPDATE ----------

// 1. findTable. nullptr -> error.
// 2. For each assignment in stmt.assignments:
//      - the column must exist
//      - it can't be the key column (index 0): changing the key would mean
//        moving the row in the tree -> error
//      - the value must be the column's type
// 3. checkWhere -> error if not "".
//    All checks happen BEFORE any row changes. If "SET age = 1, nosuch = 2"
//    fails, age must not have changed anywhere.
// 4. Collect first: go through traverseInOrder(), and for each row that
//    matches, make the new row (copy it, overwrite the assigned columns) and
//    save the (key, newRow) pair in a vector.
// 5. THEN change the tree: table->rows.update(key, newRow) for each pair.
//    Never change the tree while you're still walking through it.
// 6. return done("<n> rows updated", n);
Result Database::updateRows(const Statement& stmt) {
    Table* table = findTable(stmt.table);
    if (table == nullptr) return error("no such table " + stmt.table);
    const std::vector<Column>& columns = table->columns;

    std::vector<std::pair<int, const Value*>> changes;
    for (const auto& a : stmt.assignments) {
        int i = findColumn(columns, a.column);
        if (i < 0) return error("no such column " + a.column);
        if (i == 0) return error("the key column " + a.column + " can't be changed");
        if (a.value.type != columns[i].type)
            return error("column " + a.column + " is " + typeName(columns[i].type) +
                         ", but the value " + a.value.text + " is " + typeName(a.value.type));
        changes.emplace_back(i, &a.value);
    }

    std::string bad = checkWhere(stmt.where.get(), columns);
    if (!bad.empty()) return error(bad);

    std::vector<std::pair<Value, Row>> updates;
    auto collect = [&](const Value& key, const Row& row) {
        if (!matches(stmt.where.get(), row, columns)) return;
        Row newRow = row;
        for (const auto& [idx, value] : changes) newRow[idx] = *value;
        updates.emplace_back(key, std::move(newRow));
    };
    if (const Value* key = keyLookup(stmt.where.get(), columns)) {
        const Row* row = table->rows.find(*key);
        if (row == nullptr) return error(noRowWith(columns[0], *key));
        collect(*key, *row);
    } else {
        for (const auto& entry : table->rows.traverseInOrder()) collect(entry.first, entry.second);
    }

    for (const auto& [key, row] : updates) table->rows.update(key, row);

    size_t n = updates.size();
    return done(std::to_string(n) + " rows updated", n);
}

// ---------- DELETE ----------

// 1. findTable. nullptr -> error.
// 2. checkWhere -> error if not "".
// 3. Collect first: the KEYS of every row that matches (no WHERE = every row).
// 4. THEN table->rows.remove(key) for each one.
//    (Removing moves keys between nodes, so never remove while walking.)
// 5. return done("<n> rows deleted", n);
Result Database::deleteRows(const Statement& stmt) {
    Table* table = findTable(stmt.table);
    if (table == nullptr) return error("no such table " + stmt.table);

    std::string bad = checkWhere(stmt.where.get(), table->columns);
    if (!bad.empty()) return error(bad);

    std::vector<Value> keys;
    if (const Value* key = keyLookup(stmt.where.get(), table->columns)) {
        const Row* row = table->rows.find(*key);
        if (row == nullptr) return error(noRowWith(table->columns[0], *key));
        if (matches(stmt.where.get(), *row, table->columns)) keys.push_back(*key);
    } else {
        for (const auto& entry : table->rows.traverseInOrder())
            if (matches(stmt.where.get(), entry.second, table->columns)) keys.push_back(entry.first);
    }

    for (const Value& key : keys) table->rows.remove(key);

    size_t n = keys.size();
    return done(std::to_string(n) + " rows deleted", n);
}

// ---------- DROP / TRUNCATE ----------

// 1. If the table doesn't exist -> error.
// 2. tables.erase(stmt.table);   (unique_ptr frees the Table and its tree)
// 3. return done("Table <name> dropped", 0);
Result Database::dropTable(const Statement& stmt) {
    if (!hasTable(stmt.table)) return error("no such table " + stmt.table);
    tables.erase(stmt.table);
    return done("Table " + stmt.table + " dropped", 0);
}

// Removes every row but keeps the table and its columns.
// A BTree can't be cleared or reassigned, so swap in a brand new Table:
// 1. findTable. nullptr -> error.
// 2. n = table->rows.size();
// 3. auto fresh = std::make_unique<Table>();
//    fresh->columns = table->columns;
//    tables[stmt.table] = std::move(fresh);   (the old Table is freed here,
//                                              so don't use `table` after this)
// 4. return done("<n> rows removed", n);
Result Database::truncateTable(const Statement& stmt) {
    Table* table = findTable(stmt.table);
    if (table == nullptr) return error("no such table " + stmt.table);

    size_t n = table->rows.size();
    auto fresh = std::make_unique<Table>();
    fresh->columns = table->columns;
    tables[stmt.table] = std::move(fresh);

    return done(std::to_string(n) + " rows removed", n);
}

// ============================================================================
// Everything below is already written. You shouldn't need to change it.
// ============================================================================

// A number bigger than a long long would make std::stoll (inside compareKeys)
// throw, so reject those before any of the code above sees them.
static bool fitsInLongLong(const std::string& text) {
    try {
        size_t used = 0;
        std::stoll(text, &used);
        return used == text.size();
    } catch (const std::exception&) {
        return false;
    }
}

static std::string checkNumbers(const Expr* e) {
    if (e == nullptr) return "";
    if (e->kind != ExprKind::COMPARE) {
        std::string err = checkNumbers(e->left.get());
        return err.empty() ? checkNumbers(e->right.get()) : err;
    }
    if (e->value.type == ValueType::INT && !fitsInLongLong(e->value.text))
        return "number " + e->value.text + " is too big";
    return "";
}

static std::string checkNumbers(const Statement& stmt) {
    for (const Value& v : stmt.values)
        if (v.type == ValueType::INT && !fitsInLongLong(v.text)) return "number " + v.text + " is too big";
    for (const Assignment& a : stmt.assignments)
        if (a.value.type == ValueType::INT && !fitsInLongLong(a.value.text)) return "number " + a.value.text + " is too big";
    return checkNumbers(stmt.where.get());
}

Result Database::execute(const Statement& stmt) {
    std::string bad = checkNumbers(stmt);
    if (!bad.empty()) return error(bad);

    Result r;
    switch (stmt.type) {
        case StatementType::CREATE:   r = createTable(stmt);   break;
        case StatementType::INSERT:   r = insertRow(stmt);     break;
        case StatementType::SELECT:   r = selectRows(stmt);    break;
        case StatementType::UPDATE:   r = updateRows(stmt);    break;
        case StatementType::DELETE:   r = deleteRows(stmt);    break;
        case StatementType::DROP:     r = dropTable(stmt);     break;
        case StatementType::TRUNCATE: r = truncateTable(stmt); break;
        default:                      return error("unknown statement");
    }
    if (r.ok && stmt.type != StatementType::SELECT) dirty = true;
    return r;
}

Table* Database::findTable(const std::string& name) {
    auto it = tables.find(name);
    return it == tables.end() ? nullptr : it->second.get();
}

bool Database::hasTable(const std::string& name) const {
    return tables.count(name) != 0;
}

const Table* Database::getTable(const std::string& name) const {
    auto it = tables.find(name);
    return it == tables.end() ? nullptr : it->second.get();
}

std::vector<std::string> Database::tableNames() const {
    std::vector<std::string> names;
    for (const auto& entry : tables) names.push_back(entry.first);   // std::map is already sorted
    return names;
}

bool Database::hasUnsavedChanges() const {
    return dirty;
}

Result Database::saveAll(const std::string& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (!fs::is_directory(dir, ec)) return error("can't create folder " + dir);

    for (const auto& entry : tables) {
        std::string path = (fs::path(dir) / (entry.first + ".csv")).string();
        if (!saveTable(path, entry.second->columns, entry.second->rows))
            return error("couldn't save table " + entry.first + " to " + path);
    }

    // Delete files of tables that were dropped, or they'd come back on .load
    std::vector<fs::path> stale;
    for (const auto& file : fs::directory_iterator(dir, ec)) {
        const fs::path& p = file.path();
        if (file.is_regular_file(ec) && p.extension() == ".csv" && !hasTable(p.stem().string()))
            stale.push_back(p);
    }
    for (const fs::path& p : stale) fs::remove(p, ec);

    dirty = false;
    size_t n = tables.size();
    return done("Saved " + std::to_string(n) + (n == 1 ? " table" : " tables") + " to " + dir, 0);
}

Result Database::loadAll(const std::string& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return error("no folder called " + dir);

    std::vector<fs::path> files;
    for (const auto& file : fs::directory_iterator(dir, ec))
        if (file.is_regular_file(ec) && file.path().extension() == ".csv") files.push_back(file.path());
    if (ec) return error("couldn't read folder " + dir);
    std::sort(files.begin(), files.end());

    // Load into a separate map so a bad file leaves the current tables alone.
    std::map<std::string, std::unique_ptr<Table>> loaded;
    for (const fs::path& p : files) {
        auto table = std::make_unique<Table>();
        if (!loadTable(p.string(), table->columns, table->rows))
            return error("couldn't load " + p.string());
        loaded[p.stem().string()] = std::move(table);
    }

    tables = std::move(loaded);
    dirty = false;
    size_t n = tables.size();
    return done("Loaded " + std::to_string(n) + (n == 1 ? " table" : " tables") + " from " + dir, 0);
}

// ---------- printing ----------

// Number of characters in UTF-8 text. size() counts bytes, so 'é' would be
// 2 and throw the column borders off. Continuation bytes look like 10xxxxxx.
static size_t displayWidth(const std::string& text) {
    size_t n = 0;
    for (unsigned char c : text)
        if ((c & 0xC0) != 0x80) n++;
    return n;
}

std::string formatResult(const Result& r) {
    if (!r.ok) return "Error: " + r.message;
    if (r.header.empty()) return r.message;

    std::vector<size_t> width;
    for (const std::string& h : r.header) width.push_back(displayWidth(h));
    for (const Row& row : r.rows)
        for (size_t i = 0; i < row.size() && i < width.size(); i++)
            width[i] = std::max(width[i], displayWidth(row[i].text));

    std::ostringstream out;
    auto line = [&]() {
        out << "+";
        for (size_t w : width) out << std::string(w + 2, '-') << "+";
        out << "\n";
    };
    auto cells = [&](const std::vector<std::string>& texts, const std::vector<bool>& rightAlign) {
        out << "|";
        for (size_t i = 0; i < width.size(); i++) {
            std::string text = i < texts.size() ? texts[i] : "";
            std::string pad(width[i] - displayWidth(text), ' ');
            out << " " << (rightAlign[i] ? pad + text : text + pad) << " |";
        }
        out << "\n";
    };

    // Numbers are lined up on the right, text on the left.
    std::vector<bool> rightAlign(width.size(), false);
    if (!r.rows.empty())
        for (size_t i = 0; i < width.size() && i < r.rows[0].size(); i++)
            rightAlign[i] = r.rows[0][i].type == ValueType::INT;

    line();
    cells(r.header, std::vector<bool>(width.size(), false));
    line();
    for (const Row& row : r.rows) {
        std::vector<std::string> texts;
        for (const Value& v : row) texts.push_back(v.text);
        cells(texts, rightAlign);
    }
    if (!r.rows.empty()) line();
    out << "(" << r.message << ")";
    return out.str();
}