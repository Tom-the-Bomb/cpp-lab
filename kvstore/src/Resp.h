#pragma once
#include <cstddef>
#include <string>

#include "Tokenize.h"

enum class ParseStatus {
    Complete,
    Incomplete,
    Invalid,
};

struct ParseResult {
    ParseStatus status;
    Args args = {};
    std::size_t consumed = 0;
};

ParseResult parseResp(const std::string& buf);
