// ============================================================================
// Dennison: saving and loading tables as CSV files.
// Tests:  make test-storage
//
// Do them in this order, the tests are in the same order:
//   escapeCsvField -> parseCsv -> saveTable -> loadTable
//
// The file format is explained at the top of storage.h.
// Use tree.traverseInOrder() to get the rows and tree.insert() to put them back.
// For files: <fstream> to read/write, std::rename and std::remove from <cstdio>.
// ============================================================================
#include "storage.h"
#include <fstream>
#include <sstream>


// 1. Check if the field has a comma, a quote, \n or \r in it.
//    If not, return it as it is.
// 2. Otherwise build a new string: start with ",
//    copy each character, but write every " twice (""),
//    end with ".
//
//   bob        ->  bob
//   a,b        ->  "a,b"
//   say "hi"   ->  "say ""hi"""
std::string escapeCsvField(const std::string& field) {
    // TODO (Dennison)
    bool inQuotes = true;
    std::string newField;
    for (std::size_t i = 0; i < field.length(); i++)
    {
        char c = field[i];

        if (c == '"')
        {
            newField.push_back('"');
            inQuotes = false;
        }
        if (c == ',' || c == '\n' || c == '\r')
        {
            inQuotes = false;
        }

        newField.push_back(field[i]);
    }
    if (!inQuotes)
    {
        newField = '"' + newField + '"';
    }
    return newField;
}

// Go through the text one character at a time, keeping track of:
//   field     - the field you're building
//   record    - the fields of the current line
//   inQuotes  - are you inside a quoted field?
//
// Inside quotes:
//   "  followed by another "  -> add one " to the field, skip both
//   "  on its own             -> the quoted part is over, inQuotes = false
//   anything else (even a newline or a comma) -> add it to the field
// Outside quotes:
//   "        -> inQuotes = true (the field is quoted)
//   ,        -> the field is done: push it onto record, start a new field
//   \n       -> the field AND the record are done: push both, start a new record
//   \r\n     -> same as \n (skip the \r)
//   anything else -> add it to the field
//
// At the end:
//   if inQuotes is still true -> a quote was never closed, return false
//   if there's a line without a newline at the end, push it too
//   (but "a\n" is ONE record, not two: don't add an empty record after the
//   last newline)
//
// Remember to clear `records` at the start.
bool parseCsv(const std::string& content, std::vector<std::vector<std::string>>& records) {

    records.clear();
    bool inQuotes = false;
    std::string field;

    if (content.empty())
    {
        return true;
    }

    records.emplace_back();

    for (std::size_t i = 0; i < content.length(); i++)
    {
        if (content[i] == '"')
        {
            //if (content[i + 1] == '"')
            if (inQuotes && i + 1 < content.length() && content[i + 1] == '"')
            {
                field.push_back('"');
                i++;
            }
            else if (!inQuotes)
            {
                inQuotes = true;
            }
            else
            {
                inQuotes = false;
            }
        }
        else if (!inQuotes)
        {
            if (content[i] == ',')
            {
                records.back().push_back(field);
                field.clear();
            }
            else if (content[i] == '\n')
            {
                records.back().push_back(field);
                if (i + 1 < content.length())
                {
                    records.emplace_back();
                }

                field.clear();
            }
            else if (content[i] == '\r')
            {
                //continue;
                if (i + 1 < content.length() && content[i + 1] == '\n')
                {
                    continue;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                field.push_back(content[i]);
            }
        }
        else
        {

            field.push_back(content[i]);

        }
    }

    if (inQuotes)
    {
        return false;
    }
    else
    {
        if (!(content.back() == '\n'))
        {
            records.back().push_back(field);;
        }
        return true;
    }
}

// 1. Build the header line: "name TYPE" for each column, joined with commas.
//    e.g.  id INT,name TEXT
// 2. Open "<path>.tmp" for writing. If it doesn't open, return false.
// 3. Write the header line and a \n.
// 4. For each (key, row) in tree.traverseInOrder():
//      write escapeCsvField() of each value's text, joined with commas, then \n
// 5. Close the file. If anything went wrong while writing, delete the .tmp
//    file and return false.
// 6. std::rename(tmp, path). If that fails, delete the .tmp and return false.
//    The rename is what makes this safe: the old file is only replaced once
//    the new one is completely written.
// 7. Return true.
bool saveTable(const std::string& path, const std::vector<Column>& columns, const BTree& tree) {
    // TODO (Dennison)
    std::string tempFile = path + ".tmp";
    std::ofstream outFile(tempFile);
    std::string tempvaluetext;
    std::string tempkeytext;
    if (!outFile.is_open())
    {
        return false;
    }

    for (std::size_t i = 0; i < columns.size(); i++)
    {
        outFile << columns[i].name << " ";
        if (columns[i].type == ValueType::TEXT)
        {
            outFile << "TEXT";
        }
        else
        {
            outFile << "INT";
        }
        if (i != columns.size() - 1)
        {
            outFile << ',';
        }
    }

    outFile << '\n';

    std::vector<std::pair<Value, Row>> nodes = tree.traverseInOrder();

    for (std::size_t i = 0; i < nodes.size(); i++)
    {
        //tempvaluetext = escapeCsvField(nodes[i].second.front().text);
        //tempkeytext = escapeCsvField(nodes[i].second.back().text);
        ////size_t banana = nodes[i].second.size();
        ////outFile << banana;
        //outFile <<  nodes[i].first.text << ',' << tempvaluetext << ',' << tempkeytext << "\n";
        const Row& row = nodes[i].second;
        for (std::size_t col = 0; col < row.size(); col++)
        {
            if (col > 0)
            {
                outFile << ',';
            }

            if (row[col].type == ValueType::TEXT)
            {
                outFile << escapeCsvField(row[col].text);
            }
            else
            {
                outFile << row[col].text;
            }
        }
        outFile << '\n';
    }

    if (!outFile.good())
    {
        outFile.close();
        std::remove(tempFile.c_str());
        return false;
    }

    outFile.close();
    std::remove(path.c_str());
    if (std::rename(tempFile.c_str(), path.c_str()) != 0)
    {
        std::remove(tempFile.c_str());
        return false;
    }

    return true;
}

// Don't touch `columns` or `tree` until EVERY check has passed.
// Read into local variables first, then copy them over at the very end.
//
// 1. If tree.size() != 0, return false.
// 2. Read the whole file into a string. If it can't be opened, return false.
// 3. parseCsv it. If that fails, or there are no records, return false.
// 4. Header (records[0]): each field must be "name INT" or "name TEXT".
//    Split on the space. The name can't be empty, the type must be exactly
//    INT or TEXT. Build a local std::vector<Column>.
// 5. Every other record is a row:
//      - it must have exactly as many fields as there are columns
//      - make a Value for each field, using the column's type
//      - an INT field must be a whole number: an optional - and then only
//        digits (at least one). It also has to fit in a long long,
//        so std::stoll doesn't throw later. (Try std::stoll in a try/catch.)
//    Build a local std::vector<Row>.
// 6. No duplicate keys (the first value of each row). One easy way: insert
//    every row into a temporary BTree, insert() returns false on a duplicate.
// 7. Everything is fine: set columns = your local columns, and insert every
//    row into `tree` (key = row[0]). Return true.
bool loadTable(const std::string& path, std::vector<Column>& columns, BTree& tree) {


    if (tree.size() != 0)
    {
        return false;
    }

    std::vector<std::vector<std::string>> records;
    std::vector<Column> types;
    std::vector<Row> rows;
    BTree treetemp;

    std::ifstream in(path, std::ios::binary);

    if (!in.is_open())
    {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    if (!parseCsv(content, records) || records.empty())
    {
        return false;
    }
    if (records[0].empty())
    {
        return false;
    }

    for (size_t i = 0; i < records[0].size(); i++)
    {
        std::string data_type = records[0][i];
        size_t space = data_type.find(' ');

        if (space == std::string::npos || space == 0)
        {
            return false;
        }

        if (data_type.substr(space + 1) == "INT")
        {
            types.push_back({ ValueType::INT, data_type.substr(0, space) });
        }
        else if (data_type.substr(space + 1) == "TEXT")
        {
            types.push_back({ ValueType::TEXT, data_type.substr(0, space) });
        }
        else
        {
            return false;
        }
    }

    for (size_t i = 1; i < records.size(); i++)
    {
        if (records[i].size() != types.size())
        {
            return false;
        }

        rows.emplace_back();
        for (size_t x = 0; x < types.size(); x++)
        {


            try {
                if (types[x].type == ValueType::INT)
                {
                    const std::string& value = records[i][x];

                    if (value.empty() ||
                        value.find_first_of(" \t\n\r") != std::string::npos)
                    {
                        return false;
                    }
                    size_t pos = 0;
                    std::stoll(records[i][x], &pos);

                    if (pos != records[i][x].size())
                    {
                        return false;
                    }

                    rows.back().push_back({ ValueType::INT,  records[i][x] });
                }
                else
                {
                    rows.back().push_back({ ValueType::TEXT, records[i][x] });
                }
            }
            catch (const std::exception&)
            {
                return false;
            }
        }


        if (!treetemp.insert(rows.back().front(), rows.back()))
        {
            return false;
        }
    }
    columns = types;
    for (size_t i = 0; i < rows.size(); i++)
    {
        tree.insert(rows[i].front(), rows[i]);
    }


    return true;
}
