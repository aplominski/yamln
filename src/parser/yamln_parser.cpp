#include "yamln_parser.h"
#include "yamln_parser_error.h"

namespace yamln {

Parser::Parser(const std::string& src) : src_(src), pos_(0), line_(1), col_(1) {}

Node Parser::parse_document() {
    skip_document_start();
    Node root = parse_node(0);
    skip_whitespace_and_comments();
    if (pos_ < src_.size() && src_.substr(pos_, 3) == "...")
        pos_ += 3;
    return root;
}

void Parser::skip_document_start() {
    skip_whitespace_and_comments();
    if (pos_ + 3 <= src_.size() && src_.substr(pos_, 3) == "---") {
        pos_ += 3; col_ += 3;
        skip_to_eol();
        skip_whitespace_and_comments();
    }
}

void Parser::skip_inline_space() {
    while (!at_end() && (peek() == ' ' || peek() == '\t')) advance();
}

void Parser::skip_to_eol() {
    while (!at_end() && peek() != '\n') advance();
}

void Parser::skip_whitespace_and_comments() {
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { advance(); }
        else if (c == '#') { skip_to_eol(); }
        else break;
    }
}

void Parser::skip_inline_whitespace_and_comments() {
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t') advance();
        else if (c == '#') { skip_to_eol(); break; }
        else break;
    }
}

int Parser::current_indent() const {
    int i = 0;
    while (pos_ + i < src_.size() && src_[pos_ + i] == ' ') ++i;
    return i;
}

std::string Parser::parse_anchor_name() {
    std::string name;
    while (!at_end() && !std::isspace((unsigned char)peek()) &&
           peek() != ',' && peek() != '[' && peek() != ']' &&
           peek() != '{' && peek() != '}') {
        name += advance();
    }
    if (name.empty()) throw ParseError("Empty anchor/alias name", line_, col_);
    return name;
}

Node parse(const std::string& yaml) {
    Parser p(yaml);
    return p.parse_document();
}

} // namespace yamln
