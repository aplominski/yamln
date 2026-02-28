#pragma once

#include <string>
#include <map>
#include <optional>

#include "../../include/yamln.h"

namespace yamln {

class Parser {
public:
    explicit Parser(const std::string& src);

    Node parse_document();

    // Public accessors for helper classes
    size_t pos() const { return pos_; }
    int line() const { return line_; }
    int col() const { return col_; }
    const std::string& src() const { return src_; }
    size_t src_size() const { return src_.size(); }
    char src_at(size_t idx) const { return src_[idx]; }
    bool at_end() const { return pos_ >= src_.size(); }

    char peek(size_t offset = 0) const {
        size_t p = pos_ + offset;
        return p < src_.size() ? src_[p] : '\0';
    }

    char advance() {
        char c = src_[pos_++];
        if (c == '\n') { ++line_; col_ = 1; }
        else ++col_;
        return c;
    }

    void skip_inline_space();
    void skip_to_eol();
    void skip_whitespace_and_comments();
    void skip_inline_whitespace_and_comments();
    int current_indent() const;
    void skip_document_start();

    Node parse_node(int indent);
    std::string parse_anchor_name();

    std::map<std::string, NodeRef>& anchors() { return anchors_; }

private:
    const std::string& src_;
    size_t pos_;
    int line_, col_;
    std::map<std::string, NodeRef> anchors_;

    // Scalar parsing
    std::string parse_plain_scalar();
    std::string parse_double_quoted();
    std::string parse_single_quoted();
    std::string parse_block_scalar(char indicator, int parent_indent);
    Node coerce_scalar(const std::string& s);

    // Flow parsing
    Node parse_flow_scalar(int indent);
    Node parse_flow_sequence(int indent);
    Node parse_flow_mapping(int indent);

    // Block parsing
    Node parse_block_sequence(int indent);
    Node parse_block_mapping(int indent);
};

Node parse(const std::string& yaml);

} // namespace yamln
