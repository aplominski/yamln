#include "yamln_parser.h"
#include "yamln_parser_error.h"
#include <cassert>

namespace yamln {

Node Parser::parse_flow_scalar(int indent) {
    skip_inline_space();
    if (at_end()) return Node(nullptr);
    char c = peek();
    if (c == '[')  return parse_flow_sequence(indent);
    if (c == '{')  return parse_flow_mapping(indent);
    if (c == '"')  return Node(parse_double_quoted());
    if (c == '\'') return Node(parse_single_quoted());
    if (c == '*') {
        advance();
        std::string alias = parse_anchor_name();
        auto it = anchors_.find(alias);
        if (it == anchors_.end()) throw ParseError("Unknown alias: *" + alias, line_, col_);
        return Node(it->second);
    }
    std::string s;
    while (!at_end() && peek() != ',' && peek() != ']' && peek() != '}' && peek() != '\n') {
        if (peek() == '#' && (s.empty() || s.back() == ' ')) break;
        s += advance();
    }
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return coerce_scalar(s);
}

Node Parser::parse_flow_sequence(int indent) {
    assert(peek() == '[');
    advance();
    Sequence seq;
    skip_whitespace_and_comments();
    while (!at_end() && peek() != ']') {
        seq.push_back(parse_flow_scalar(indent));
        skip_whitespace_and_comments();
        if (peek() == ',') { advance(); skip_whitespace_and_comments(); }
    }
    if (at_end()) throw ParseError("Unterminated flow sequence", line_, col_);
    advance();
    return Node(seq);
}

Node Parser::parse_flow_mapping(int indent) {
    assert(peek() == '{');
    advance();
    Mapping map;
    skip_whitespace_and_comments();
    while (!at_end() && peek() != '}') {
        std::string key;
        if (peek() == '"')       key = parse_double_quoted();
        else if (peek() == '\'') key = parse_single_quoted();
        else {
            while (!at_end() && peek() != ':' && peek() != '}' && peek() != '\n')
                key += advance();
            while (!key.empty() && key.back() == ' ') key.pop_back();
        }
        skip_inline_space();
        if (peek() != ':') throw ParseError("Expected ':' in flow mapping", line_, col_);
        advance();
        skip_inline_space();
        map[key] = parse_flow_scalar(indent);
        skip_whitespace_and_comments();
        if (peek() == ',') { advance(); skip_whitespace_and_comments(); }
    }
    if (at_end()) throw ParseError("Unterminated flow mapping", line_, col_);
    advance();
    return Node(map);
}

} // namespace yamln
