#pragma once
#include <stdexcept>
#include <string>

namespace yamln {

struct ParseError : std::runtime_error {
    ParseError(const std::string& msg, int line, int col)
        : std::runtime_error("YAML parse error at line " + std::to_string(line) +
                             ", col " + std::to_string(col) + ": " + msg) {}
};

} // namespace yamln
