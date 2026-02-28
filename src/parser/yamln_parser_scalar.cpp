#include "yamln_parser.h"
#include "yamln_parser_error.h"
#include <cctype>
#include <cstdlib>
#include <cassert>

namespace yamln {

std::string Parser::parse_plain_scalar() {
    std::string result;
    while (!at_end()) {
        char c = peek();
        if (c == ':' && (peek(1) == ' ' || peek(1) == '\t' || peek(1) == '\n' || peek(1) == '\0'))
            break;
        if (c == '#' && !result.empty() && (result.back() == ' ' || result.back() == '\t'))
            break;
        if (c == '\n' || c == '\r') break;
        if (c == ',' || c == '}' || c == ']') break;
        result += advance();
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '\t'))
        result.pop_back();
    return result;
}

std::string Parser::parse_double_quoted() {
    assert(peek() == '"');
    advance();
    std::string result;
    while (!at_end() && peek() != '"') {
        char c = advance();
        if (c == '\\') {
            if (at_end()) throw ParseError("Unterminated escape sequence", line_, col_);
            char e = advance();
            switch (e) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '0':  result += '\0'; break;
                default:   result += '\\'; result += e; break;
            }
        } else {
            result += c;
        }
    }
    if (at_end()) throw ParseError("Unterminated double-quoted string", line_, col_);
    advance();
    return result;
}

std::string Parser::parse_single_quoted() {
    assert(peek() == '\'');
    advance();
    std::string result;
    while (!at_end()) {
        char c = advance();
        if (c == '\'') {
            if (peek() == '\'') { result += '\''; advance(); }
            else break;
        } else {
            result += c;
        }
    }
    return result;
}

std::string Parser::parse_block_scalar(char indicator, int parent_indent) {
    advance();
    char chomp = 'c';
    if (!at_end() && (peek() == '-' || peek() == '+')) chomp = advance();
    while (!at_end() && std::isdigit((unsigned char)peek())) advance();
    skip_inline_whitespace_and_comments();
    if (!at_end() && peek() == '\n') advance();

    int block_indent = -1;
    std::string result;
    std::string trailing_newlines;

    while (!at_end()) {
        size_t tmp_pos = pos_;
        int ind = 0;
        while (tmp_pos < src_.size() && src_[tmp_pos] == ' ') { ++ind; ++tmp_pos; }

        if (tmp_pos >= src_.size() || src_[tmp_pos] == '\n' || src_[tmp_pos] == '\r') {
            while (!at_end() && peek() != '\n') advance();
            if (!at_end()) advance();
            trailing_newlines += '\n';
            continue;
        }

        if (block_indent == -1) {
            if (ind <= parent_indent) break;
            block_indent = ind;
        }
        if (ind < block_indent) break;

        result += trailing_newlines;
        trailing_newlines.clear();

        for (int i = 0; i < block_indent; ++i) advance();

        std::string line_content;
        while (!at_end() && peek() != '\n' && peek() != '\r') line_content += advance();
        if (!at_end() && peek() == '\r') advance();
        if (!at_end() && peek() == '\n') advance();

        if (indicator == '>') {
            if (!result.empty() && result.back() != '\n') result += ' ';
            result += line_content;
        } else {
            result += line_content + '\n';
        }
    }

    if (chomp == '-') {
        while (!result.empty() && result.back() == '\n') result.pop_back();
    } else if (chomp == '+') {
        result += trailing_newlines;
    } else {
        while (!result.empty() && result.back() == '\n') result.pop_back();
        result += '\n';
    }
    return result;
}

Node Parser::coerce_scalar(const std::string& s) {
    if (s == "null" || s == "~" || s.empty()) return Node(nullptr);
    if (s == "true"  || s == "True"  || s == "TRUE")  return Node(true);
    if (s == "false" || s == "False" || s == "FALSE") return Node(false);

    {
        size_t i = 0;
        if (s[0] == '-' || s[0] == '+') ++i;
        bool all_digits = (i < s.size());
        for (size_t j = i; j < s.size(); ++j) {
            if (!std::isdigit((unsigned char)s[j])) { all_digits = false; break; }
        }
        if (all_digits && i < s.size()) {
            try { return Node(std::stoi(s)); } catch (...) {}
        }
    }

    {
        char* end = nullptr;
        double d = std::strtod(s.c_str(), &end);
        if (end && end != s.c_str() && *end == '\0') return Node(d);
    }

    return Node(s);
}

} // namespace yamln
