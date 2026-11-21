// ============================================================================
// Rajeev: the executor (runs parsed statements against the tables).
// Tests:  make test-executor
//
// Do them in this order, the tests are in the same order:
//   (createTable is done) -> insertRow -> selectRows -> matches + checkWhere (WHERE)
//   -> updateRows -> deleteRows -> dropTable / truncateTable -> ORDER BY
//
// The checks ("validation") go at the top of each function, BEFORE anything
// is changed. A statement that fails must leave every table exactly as it was.
//
// Already written for you (bottom of the file): execute(), the table lookups,
// saveAll / loadAll and formatResult.
//
// Useful things that already exist:
//   error("...")              -> a failed Result with that message
//   done("...", n)            -> a successful Result with affected = n
//   findColumn(columns, name) -> index of the column, or -1
//   compareKeys(a, b)         -> from btree.h, < 0 / 0 / > 0, INT as numbers
//   table->rows.insert / find / update / remove / traverseInOrder / size
// ============================================================================
#include "executor.h"
#include "storage.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

// ---------- small helpers (already written) ----------

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

// Index of the column called `name`, or -1 if there isn't one.
[[maybe_unused]] static int findColumn(const std::vector<Column>& columns, const std::string& name) {
    for (size_t i = 0; i < columns.size(); i++)
        if (columns[i].name == name) return static_cast<int>(i);
    return -1;
}

[[maybe_unused]] static const char* typeName(ValueType t) { return t == ValueType::INT ? "INT" : "TEXT"; }

// ---------- CREATE (done, use it as the example for the rest) ----------

// The pattern every statement follows:
//   checks first (return error(...) on the first problem),
//   change things only once every check has passed,
//   then return done(...).
Result Database::createTable(const Statement& stmt) {
    // 1. the table can't already exist
    if (hasTable(stmt.table)) return error("table " + stmt.table + " already exists");

    // 2. no two columns with the same name: CREATE TABLE t (a INT, a TEXT)
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
    // TODO (Rajeev)
    (void)stmt;
    return error("INSERT is not written yet");
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
    // TODO (Rajeev)
    (void)where;
    (void)columns;
    return "checkWhere is not written yet";
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
    // TODO (Rajeev)
    (void)where;
    (void)row;
    (void)columns;
    return false;
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
    // TODO (Rajeev)
    (void)stmt;
    return error("SELECT is not written yet");
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
    // TODO (Rajeev)
    (void)stmt;
    return error("UPDATE is not written yet");
}

// ---------- DELETE ----------

// 1. findTable. nullptr -> error.
// 2. checkWhere -> error if not "".
// 3. Collect first: the KEYS of every row that matches (no WHERE = every row).
// 4. THEN table->rows.remove(key) for each one.
//    (Removing moves keys between nodes, so never remove while walking.)
// 5. return done("<n> rows deleted", n);
Result Database::deleteRows(const Statement& stmt) {
    // TODO (Rajeev)
    (void)stmt;
    return error("DELETE is not written yet");
}

// ---------- DROP / TRUNCATE ----------

// 1. If the table doesn't exist -> error.
// 2. tables.erase(stmt.table);   (unique_ptr frees the Table and its tree)
// 3. return done("Table <name> dropped", 0);
Result Database::dropTable(const Statement& stmt) {
    // TODO (Rajeev)
    (void)stmt;
    return error("DROP is not written yet");
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
    // TODO (Rajeev)
    (void)stmt;
    return error("TRUNCATE is not written yet");
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

std::string formatResult(const Result& r) {
    if (!r.ok) return "Error: " + r.message;
    if (r.header.empty()) return r.message;

    std::vector<size_t> width;
    for (const std::string& h : r.header) width.push_back(h.size());
    for (const Row& row : r.rows)
        for (size_t i = 0; i < row.size() && i < width.size(); i++)
            width[i] = std::max(width[i], row[i].text.size());

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
            std::string pad(width[i] - text.size(), ' ');
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
    out << "(" << r.rows.size() << (r.rows.size() == 1 ? " row)" : " rows)");
    return out.str();
}
