#include "yamln_parser.h"
#include "yamln_parser_error.h"

namespace yamln {

Node Parser::parse_block_sequence(int indent) {
    Sequence seq;
    while (!at_end()) {
        {
            size_t saved = pos_; int sl = line_, sc = col_;
            skip_whitespace_and_comments();
            if (at_end()) break;
            int ind = col_ - 1;
            if (peek() != '-') { pos_ = saved; line_ = sl; col_ = sc; break; }
            if (ind != indent)  { pos_ = saved; line_ = sl; col_ = sc; break; }
        }
        if (peek(1) != ' ' && peek(1) != '\t' && peek(1) != '\n' && peek(1) != '\0') break;
        advance();
        if (!at_end() && (peek() == ' ' || peek() == '\t')) advance();
        skip_inline_space();

        if (!at_end() && (peek() == '\n' || peek() == '\r' || peek() == '#')) {
            skip_inline_whitespace_and_comments();
            if (!at_end() && peek() == '\n') advance();
            size_t saved = pos_; int sl = line_, sc = col_;
            skip_whitespace_and_comments();
            if (at_end() || col_ - 1 <= indent) {
                pos_ = saved; line_ = sl; col_ = sc;
                seq.push_back(Node(nullptr));
            } else {
                seq.push_back(parse_node(col_ - 1));
            }
        } else {
            seq.push_back(parse_node(indent));
            skip_inline_whitespace_and_comments();
            if (!at_end() && (peek() == '\n' || peek() == '\r')) advance();
        }
    }
    return Node(seq);
}

Node Parser::parse_block_mapping(int indent) {
    Mapping map;
    while (!at_end()) {
        {
            size_t saved = pos_; int sl = line_, sc = col_;
            skip_whitespace_and_comments();
            if (at_end()) break;
            int ind = col_ - 1;
            if (ind < indent) { pos_ = saved; line_ = sl; col_ = sc; break; }
            if (ind > indent) { pos_ = saved; line_ = sl; col_ = sc; break; }
        }

        std::string key;
        if (peek() == '"') {
            key = parse_double_quoted();
        } else if (peek() == '\'') {
            key = parse_single_quoted();
        } else {
            size_t p = pos_;
            bool found_colon = false;
            while (p < src_.size() && src_[p] != '\n') {
                if (src_[p] == ':' &&
                    (p + 1 >= src_.size() ||
                     src_[p+1] == ' ' || src_[p+1] == '\t' ||
                     src_[p+1] == '\n' || src_[p+1] == '\0')) {
                    found_colon = true;
                    break;
                }
                ++p;
            }
            if (!found_colon) break;
            while (!at_end() && peek() != ':' && peek() != '\n') key += advance();
            while (!key.empty() && key.back() == ' ') key.pop_back();
        }

        skip_inline_space();
        if (at_end() || peek() != ':')
            throw ParseError("Expected ':' after key '" + key + "'", line_, col_);
        advance();
        skip_inline_space();

        Node value;
        if (!at_end() && (peek() == '\n' || peek() == '\r' || peek() == '#')) {
            skip_inline_whitespace_and_comments();
            if (!at_end() && peek() == '\n') advance();
            size_t saved = pos_; int sl = line_, sc = col_;
            skip_whitespace_and_comments();
            if (at_end() || col_ - 1 <= indent) {
                pos_ = saved; line_ = sl; col_ = sc;
                value = Node(nullptr);
            } else {
                value = parse_node(col_ - 1);
            }
        } else {
            value = parse_node(indent);
            skip_inline_whitespace_and_comments();
            if (!at_end() && (peek() == '\n' || peek() == '\r')) advance();
        }
        map[key] = std::move(value);
    }
    return Node(map);
}

Node Parser::parse_node(int indent) {
    skip_inline_space();

    std::optional<std::string> anchor_name;
    if (!at_end() && peek() == '&') {
        advance();
        anchor_name = parse_anchor_name();
        skip_inline_space();
    }

    if (!at_end() && peek() == '*') {
        advance();
        std::string alias = parse_anchor_name();
        auto it = anchors_.find(alias);
        if (it == anchors_.end()) throw ParseError("Unknown alias: *" + alias, line_, col_);
        return Node(it->second);
    }

    Node result;

    if (at_end()) {
        result = Node(nullptr);
    } else if (peek() == '\n' || peek() == '\r') {
        if (anchor_name) {
            skip_inline_whitespace_and_comments();
            if (!at_end() && peek() == '\n') advance();
            size_t saved = pos_; int sl = line_, sc = col_;
            skip_whitespace_and_comments();
            if (!at_end() && col_ - 1 > indent) {
                result = parse_node(col_ - 1);
            } else {
                pos_ = saved; line_ = sl; col_ = sc;
                result = Node(nullptr);
            }
        } else {
            result = Node(nullptr);
        }
    } else if (peek() == '[') {
        result = parse_flow_sequence(indent);
    } else if (peek() == '{') {
        result = parse_flow_mapping(indent);
    } else if (peek() == '"') {
        result = Node(parse_double_quoted());
    } else if (peek() == '\'') {
        result = Node(parse_single_quoted());
    } else if (peek() == '|' || peek() == '>') {
        char blk = peek();
        result = Node(parse_block_scalar(blk, indent));
    } else if (peek() == '-' && (peek(1) == ' ' || peek(1) == '\t' || peek(1) == '\n' || peek(1) == '\0')) {
        int seq_indent = col_ - 1;
        result = parse_block_sequence(seq_indent);
    } else {
        size_t p = pos_;
        bool is_map_key = false;
        while (p < src_.size() && src_[p] != '\n') {
            if (src_[p] == ':' && (p + 1 >= src_.size() ||
                src_[p+1] == ' ' || src_[p+1] == '\t' ||
                src_[p+1] == '\n' || src_[p+1] == '\0')) {
                is_map_key = true;
                break;
            }
            ++p;
        }
        if (is_map_key) {
            result = parse_block_mapping(col_ - 1);
        } else {
            result = coerce_scalar(parse_plain_scalar());
        }
    }

    if (anchor_name) {
        result.anchor = anchor_name;
        anchors_[*anchor_name] = std::make_shared<Node>(result);
    }

    return result;
}

} // namespace yamln
