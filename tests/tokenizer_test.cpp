// Tokenizer tests. Run with:  make test
//
// Tests go from easiest to hardest. Work top to bottom:
// make the first failing test pass, re-run, repeat.

#include "../src/tokenizer.h"
#include <iostream>
#include <string>
#include <vector>

using T = TokenType;

struct Expected {
    TokenType type;
    std::string text = "";  // only checked for IDENTIFIER, NUMBER, STRING
};

static int passed = 0;
static int failed = 0;

static std::string show(const std::vector<Token>& tokens) {
    std::string out;
    for (const Token& t : tokens) {
        out += tokenTypeName(t.type);
        if (t.type == T::IDENTIFIER || t.type == T::NUMBER || t.type == T::STRING)
            out += "(" + t.text + ")";
        out += " ";
    }
    return out;
}

static std::string show(const std::vector<Expected>& tokens) {
    std::vector<Token> asTokens;
    for (const Expected& e : tokens) asTokens.push_back({e.type, e.text});
    return show(asTokens);
}

static void check(const std::string& name, const std::string& input,
                  const std::vector<Expected>& expected) {
    std::vector<Token> actual = tokenize(input);

    bool ok = actual.size() == expected.size();
    for (size_t i = 0; ok && i < actual.size(); i++) {
        if (actual[i].type != expected[i].type) ok = false;
        T type = expected[i].type;
        bool checkText = type == T::IDENTIFIER || type == T::NUMBER || type == T::STRING;
        if (checkText && actual[i].text != expected[i].text) ok = false;
    }

    if (ok) {
        passed++;
        std::cout << "  PASS  " << name << "\n";
    } else {
        failed++;
        std::cout << "  FAIL  " << name << "\n"
                  << "        input:    " << input << "\n"
                  << "        expected: " << show(expected) << "\n"
                  << "        actual:   " << show(actual) << "\n";
    }
}

int main() {
    std::cout << "\n-- 1. Empty input --\n";
    check("empty string gives just END", "", {{T::END}});
    check("only spaces gives just END", "   ", {{T::END}});
    check("tabs and newlines are skipped", "\t\n ", {{T::END}});

    std::cout << "\n-- 2. Single-character symbols --\n";
    check("star", "*", {{T::STAR}, {T::END}});
    check("comma", ",", {{T::COMMA}, {T::END}});
    check("parentheses", "()", {{T::LPAREN}, {T::RPAREN}, {T::END}});
    check("semicolon", ";", {{T::SEMICOLON}, {T::END}});
    check("equal", "=", {{T::EQUAL}, {T::END}});
    check("less", "<", {{T::LESS}, {T::END}});
    check("greater", ">", {{T::GREATER}, {T::END}});
    check("symbols with spaces between", " * , ; ",
          {{T::STAR}, {T::COMMA}, {T::SEMICOLON}, {T::END}});

    std::cout << "\n-- 3. Two-character symbols (peek at the next char) --\n";
    check("less or equal", "<=", {{T::LESS_EQUAL}, {T::END}});
    check("greater or equal", ">=", {{T::GREATER_EQUAL}, {T::END}});
    check("not equal", "!=", {{T::NOT_EQUAL}, {T::END}});
    check("lone ! is an error", "!", {{T::ERROR}, {T::END}});
    check("< then space then = is two tokens", "< =", {{T::LESS}, {T::EQUAL}, {T::END}});

    std::cout << "\n-- 4. Numbers --\n";
    check("single digit", "7", {{T::NUMBER, "7"}, {T::END}});
    check("multiple digits", "2026", {{T::NUMBER, "2026"}, {T::END}});
    check("negative number", "-5", {{T::NUMBER, "-5"}, {T::END}});
    check("two numbers", "1 2", {{T::NUMBER, "1"}, {T::NUMBER, "2"}, {T::END}});

    std::cout << "\n-- 5. Identifiers --\n";
    check("simple name", "users", {{T::IDENTIFIER, "users"}, {T::END}});
    check("name with underscore", "user_name", {{T::IDENTIFIER, "user_name"}, {T::END}});
    check("name with digits", "age2", {{T::IDENTIFIER, "age2"}, {T::END}});
    check("name keeps its original case", "Users", {{T::IDENTIFIER, "Users"}, {T::END}});

    std::cout << "\n-- 6. Keywords --\n";
    check("SELECT", "SELECT", {{T::SELECT}, {T::END}});
    check("lowercase select", "select", {{T::SELECT}, {T::END}});
    check("mixed case SeLeCt", "SeLeCt", {{T::SELECT}, {T::END}});
    check("all keywords",
          "CREATE TABLE INSERT INTO VALUES SELECT FROM WHERE UPDATE SET DELETE AND OR INT TEXT",
          {{T::CREATE}, {T::TABLE}, {T::INSERT}, {T::INTO}, {T::VALUES}, {T::SELECT},
           {T::FROM}, {T::WHERE}, {T::UPDATE}, {T::SET}, {T::DELETE}, {T::AND},
           {T::OR}, {T::INT}, {T::TEXT}, {T::END}});
    check("word starting with a keyword is an identifier", "selected",
          {{T::IDENTIFIER, "selected"}, {T::END}});
    check("word ending with a keyword is an identifier", "myfrom",
          {{T::IDENTIFIER, "myfrom"}, {T::END}});

    std::cout << "\n-- 7. Strings --\n";
    check("simple string, quotes removed", "'bob'", {{T::STRING, "bob"}, {T::END}});
    check("string with spaces", "'hello world'", {{T::STRING, "hello world"}, {T::END}});
    check("empty string", "''", {{T::STRING, ""}, {T::END}});
    check("keyword inside a string stays a string", "'SELECT'",
          {{T::STRING, "SELECT"}, {T::END}});
    check("missing closing quote is an error", "'bob", {{T::ERROR}, {T::END}});

    std::cout << "\n-- 8. Errors --\n";
    check("unknown character", "@", {{T::ERROR}, {T::END}});
    check("error in the middle keeps going", "SELECT @ FROM x",
          {{T::SELECT}, {T::ERROR}, {T::FROM}, {T::IDENTIFIER, "x"}, {T::END}});

    std::cout << "\n-- 9. No spaces between tokens --\n";
    check("age>=18", "age>=18",
          {{T::IDENTIFIER, "age"}, {T::GREATER_EQUAL}, {T::NUMBER, "18"}, {T::END}});
    check("(1,'bob')", "(1,'bob')",
          {{T::LPAREN}, {T::NUMBER, "1"}, {T::COMMA}, {T::STRING, "bob"}, {T::RPAREN}, {T::END}});
    check("users;", "users;", {{T::IDENTIFIER, "users"}, {T::SEMICOLON}, {T::END}});

    std::cout << "\n-- 10. Full commands --\n";
    check("CREATE TABLE", "CREATE TABLE users (id INT, name TEXT, age INT);",
          {{T::CREATE}, {T::TABLE}, {T::IDENTIFIER, "users"}, {T::LPAREN},
           {T::IDENTIFIER, "id"}, {T::INT}, {T::COMMA},
           {T::IDENTIFIER, "name"}, {T::TEXT}, {T::COMMA},
           {T::IDENTIFIER, "age"}, {T::INT}, {T::RPAREN}, {T::SEMICOLON}, {T::END}});

    check("INSERT", "INSERT INTO users VALUES (1, 'bob', 20);",
          {{T::INSERT}, {T::INTO}, {T::IDENTIFIER, "users"}, {T::VALUES}, {T::LPAREN},
           {T::NUMBER, "1"}, {T::COMMA}, {T::STRING, "bob"}, {T::COMMA},
           {T::NUMBER, "20"}, {T::RPAREN}, {T::SEMICOLON}, {T::END}});

    check("SELECT with WHERE", "SELECT name FROM users WHERE age >= 18;",
          {{T::SELECT}, {T::IDENTIFIER, "name"}, {T::FROM}, {T::IDENTIFIER, "users"},
           {T::WHERE}, {T::IDENTIFIER, "age"}, {T::GREATER_EQUAL}, {T::NUMBER, "18"},
           {T::SEMICOLON}, {T::END}});

    check("SELECT * with AND", "SELECT * FROM users WHERE age > 18 AND name != 'bob';",
          {{T::SELECT}, {T::STAR}, {T::FROM}, {T::IDENTIFIER, "users"}, {T::WHERE},
           {T::IDENTIFIER, "age"}, {T::GREATER}, {T::NUMBER, "18"}, {T::AND},
           {T::IDENTIFIER, "name"}, {T::NOT_EQUAL}, {T::STRING, "bob"},
           {T::SEMICOLON}, {T::END}});

    check("UPDATE", "UPDATE users SET age = 21 WHERE id = 1;",
          {{T::UPDATE}, {T::IDENTIFIER, "users"}, {T::SET}, {T::IDENTIFIER, "age"},
           {T::EQUAL}, {T::NUMBER, "21"}, {T::WHERE}, {T::IDENTIFIER, "id"},
           {T::EQUAL}, {T::NUMBER, "1"}, {T::SEMICOLON}, {T::END}});

    check("DELETE", "delete from users where id <= 3 or name = 'x';",
          {{T::DELETE}, {T::FROM}, {T::IDENTIFIER, "users"}, {T::WHERE},
           {T::IDENTIFIER, "id"}, {T::LESS_EQUAL}, {T::NUMBER, "3"}, {T::OR},
           {T::IDENTIFIER, "name"}, {T::EQUAL}, {T::STRING, "x"},
           {T::SEMICOLON}, {T::END}});

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
