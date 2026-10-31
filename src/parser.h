#pragma once
#include "statement.h"
#include "tokenizer.h"
#include <stdexcept>
#include <vector>

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

Statement parse(const std::vector<Token>& tokens);