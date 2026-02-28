#include "yamln.h"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <cassert>

namespace yamln {

// ============================================================
//  Parser
// ============================================================

struct ParseError : std::runtime_error {
    ParseError(const std::string& msg, int line, int col)
        : std::runtime_error("YAML parse error at line " + std::to_string(line) +
                             ", col " + std::to_string(col) + ": " + msg) {}
};

class Parser {
public:
    explicit Parser(const std::string& src) : src_(src), pos_(0), line_(1), col_(1) {}

    Node parse_document() {
        skip_document_start();
        Node root = parse_node(0);
        skip_whitespace_and_comments();
        if (pos_ < src_.size() && src_.substr(pos_, 3) == "...")
            pos_ += 3;
        return root;
    }

private:
    const std::string& src_;
    size_t pos_;
    int line_, col_;
    std::map<std::string, NodeRef> anchors_;

    // ---- source primitives ----

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

    bool at_end() const { return pos_ >= src_.size(); }

    void skip_inline_space() {
        while (!at_end() && (peek() == ' ' || peek() == '\t')) advance();
    }

    void skip_to_eol() {
        while (!at_end() && peek() != '\n') advance();
    }

    // Skip all whitespace (including newlines) and full-line comments
    void skip_whitespace_and_comments() {
        while (!at_end()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { advance(); }
            else if (c == '#') { skip_to_eol(); }
            else break;
        }
    }

    // Skip spaces/tabs and inline comments (do NOT skip newlines)
    void skip_inline_whitespace_and_comments() {
        while (!at_end()) {
            char c = peek();
            if (c == ' ' || c == '\t') advance();
            else if (c == '#') { skip_to_eol(); break; }
            else break;
        }
    }

    // Count leading spaces on the current line (without advancing)
    int current_indent() const {
        int i = 0;
        while (pos_ + i < src_.size() && src_[pos_ + i] == ' ') ++i;
        return i;
    }

    void skip_document_start() {
        skip_whitespace_and_comments();
        if (pos_ + 3 <= src_.size() && src_.substr(pos_, 3) == "---") {
            pos_ += 3; col_ += 3;
            skip_to_eol();
            skip_whitespace_and_comments();
        }
    }

    // ---- scalar parsing ----

    std::string parse_plain_scalar() {
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

    std::string parse_double_quoted() {
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
        advance(); // closing "
        return result;
    }

    std::string parse_single_quoted() {
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

    // Parse block scalar (| or >) - simplified chomping
    std::string parse_block_scalar(char indicator, int parent_indent) {
        advance(); // consume | or >
        char chomp = 'c'; // clip by default
        if (!at_end() && (peek() == '-' || peek() == '+')) chomp = advance();
        while (!at_end() && std::isdigit((unsigned char)peek())) advance(); // skip explicit indent
        skip_inline_whitespace_and_comments();
        if (!at_end() && peek() == '\n') advance();

        int block_indent = -1;
        std::string result;
        std::string trailing_newlines;

        while (!at_end()) {
            // Count spaces on this line
            size_t tmp_pos = pos_;
            int ind = 0;
            while (tmp_pos < src_.size() && src_[tmp_pos] == ' ') { ++ind; ++tmp_pos; }

            // Empty or blank line
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

        // Chomping
        if (chomp == '-') {
            while (!result.empty() && result.back() == '\n') result.pop_back();
        } else if (chomp == '+') {
            result += trailing_newlines;
        } else {
            // clip: exactly one trailing newline
            while (!result.empty() && result.back() == '\n') result.pop_back();
            result += '\n';
        }
        return result;
    }

    // ---- type coercion ----

    Node coerce_scalar(const std::string& s) {
        if (s == "null" || s == "~" || s.empty()) return Node(nullptr);
        if (s == "true"  || s == "True"  || s == "TRUE")  return Node(true);
        if (s == "false" || s == "False" || s == "FALSE") return Node(false);

        // Try int
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

        // Try double
        {
            char* end = nullptr;
            double d = std::strtod(s.c_str(), &end);
            if (end && end != s.c_str() && *end == '\0') return Node(d);
        }

        return Node(s);
    }

    // ---- flow context scalars (stop at , ] }) ----

    Node parse_flow_scalar(int indent) {
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
        // Plain flow scalar
        std::string s;
        while (!at_end() && peek() != ',' && peek() != ']' && peek() != '}' && peek() != '\n') {
            if (peek() == '#' && (s.empty() || s.back() == ' ')) break;
            s += advance();
        }
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return coerce_scalar(s);
    }

    Node parse_flow_sequence(int indent) {
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
        advance(); // ]
        return Node(seq);
    }

    Node parse_flow_mapping(int indent) {
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
        advance(); // }
        return Node(map);
    }

    // ---- anchor / alias helpers ----

    std::string parse_anchor_name() {
        std::string name;
        while (!at_end() && !std::isspace((unsigned char)peek()) &&
               peek() != ',' && peek() != '[' && peek() != ']' &&
               peek() != '{' && peek() != '}') {
            name += advance();
        }
        if (name.empty()) throw ParseError("Empty anchor/alias name", line_, col_);
        return name;
    }

    // ---- block sequence ----
    // Called when pos_ is AT the '-' of the first item.
    // `indent` = 0-indexed column of the '-' markers.
    Node parse_block_sequence(int indent) {
        Sequence seq;
        while (!at_end()) {
            {
                size_t saved = pos_; int sl = line_, sc = col_;
                skip_whitespace_and_comments();
                if (at_end()) break;
                int ind = col_ - 1; // 0-indexed column
                if (peek() != '-') { pos_ = saved; line_ = sl; col_ = sc; break; }
                if (ind != indent)  { pos_ = saved; line_ = sl; col_ = sc; break; }
            }
            // Ensure it's actually a sequence item, not e.g. a negative number
            if (peek(1) != ' ' && peek(1) != '\t' && peek(1) != '\n' && peek(1) != '\0') break;
            advance(); // consume '-'
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

    // ---- block mapping ----
    // Called when pos_ is AT the first char of the first key (indent spaces already consumed).
    // `indent` = 0-indexed column of the keys.
    Node parse_block_mapping(int indent) {
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
            // Now at correct indent, peek at first char of key

            std::string key;
            if (peek() == '"') {
                key = parse_double_quoted();
            } else if (peek() == '\'') {
                key = parse_single_quoted();
            } else {
                // Verify colon exists on this line
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
            advance(); // ':'
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
                // Inline value - pass key's indent so block scalars use correct parent_indent
                value = parse_node(indent);
                skip_inline_whitespace_and_comments();
                if (!at_end() && (peek() == '\n' || peek() == '\r')) advance();
            }
            map[key] = std::move(value);
        }
        return Node(map);
    }

    // ---- main node dispatch ----
    // `indent` = the indent of the containing key (for block scalars and block children).
    // pos_ is positioned at the first non-space character of the value
    // (for inline values, caller already consumed leading spaces).
    Node parse_node(int indent) {
        skip_inline_space();

        // Anchor handling
        std::optional<std::string> anchor_name;
        if (!at_end() && peek() == '&') {
            advance();
            anchor_name = parse_anchor_name();
            skip_inline_space();
        }

        // Alias
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
            // Anchor on same line as key, actual value is the block below
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
            // Check for block mapping (colon on this line)
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
};

Node parse(const std::string& yaml) {
    Parser p(yaml);
    return p.parse_document();
}

} // namespace yamln