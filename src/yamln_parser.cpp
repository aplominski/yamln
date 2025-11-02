#include <string>
#include <variant>
#include <vector>
#include <map>
#include <stdexcept>
#include <optional>
#include <memory>
#include <sstream>
#include <functional>

#include <yamln.h>
namespace yamln {

__attribute__((visibility("default"))) Node parse(const std::string& yaml) {
    std::vector<std::string> lines;
    std::istringstream iss(yaml);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    auto get_indent = [](const std::string& l) -> int {
        int count = 0;
        for (char c : l) {
            if (c != ' ') break;
            count++;
        }
        return count;
    };

    auto trim = [](const std::string& s) -> std::string {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    };

    auto trim_left = [](const std::string& s) -> std::string {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        return s.substr(start);
    };

    auto get_content = [&get_indent, &trim](const std::string& l) -> std::string {
        int ind = get_indent(l);
        std::string cont = l.substr(ind);
        size_t hash = cont.find('#');
        if (hash != std::string::npos) cont = cont.substr(0, hash);
        size_t end = cont.find_last_not_of(" \t");
        if (end == std::string::npos) return "";
        return cont.substr(0, end + 1);
    };

    auto skip_empty_lines = [&lines, &get_content](size_t& idx) {
        while (idx < lines.size() && get_content(lines[idx]).empty()) {
            idx++;
        }
    };

    auto parse_scalar = [&trim](std::string cont) -> Node {
        cont = trim(cont);
        if (cont == "~" || cont == "null" || cont.empty()) return Node(nullptr);
        if (cont == "true") return Node(true);
        if (cont == "false") return Node(false);
        try {
            if (cont.find('.') != std::string::npos || cont.find('e') != std::string::npos || cont.find('E') != std::string::npos) {
                return Node(std::stod(cont));
            } else {
                return Node(std::stoi(cont));
            }
        } catch (...) {
            return Node(cont);
        }
    };

    std::function<Node(size_t&, int, std::map<std::string, NodeRef>&)> parse_sequence;
    std::function<Node(size_t&, int, std::map<std::string, NodeRef>&)> parse_mapping;
    std::function<Node(size_t&, int, std::map<std::string, NodeRef>&)> parse_node;

    parse_sequence = [&lines, &skip_empty_lines, &get_content, &get_indent, &trim_left, &parse_scalar, &parse_node](size_t& idx, int cur_ind, std::map<std::string, NodeRef>& anch) -> Node {
        Sequence seq;
        while (idx < lines.size()) {
            skip_empty_lines(idx);
            if (idx >= lines.size()) break;
            int th_ind = get_indent(lines[idx]);
            if (th_ind < cur_ind) break;
            if (th_ind > cur_ind) throw std::runtime_error("Indent error in sequence");
            std::string cnt = get_content(lines[idx]);
            if (cnt.compare(0, 1, "-") != 0) break;
            std::optional<std::string> it_anch;
            std::string it_cnt = trim_left(cnt.substr(1));
            if (it_cnt.compare(0, 1, "&") == 0) {
                size_t sp = it_cnt.find(' ');
                if (sp == std::string::npos) sp = it_cnt.size();
                it_anch = it_cnt.substr(1, sp - 1);
                it_cnt = trim_left(it_cnt.substr(sp));
            }
            Node item;
            bool on_same_line = !it_cnt.empty();
            if (it_cnt.compare(0, 1, "*") == 0) {
                std::string al = it_cnt.substr(1);
                auto it = anch.find(al);
                if (it == anch.end()) throw std::runtime_error("Unknown alias: " + al);
                item = Node(it->second);
            } else if (on_same_line) {
                item = parse_scalar(it_cnt);
            } else {
                idx++;
                skip_empty_lines(idx);
                if (idx >= lines.size()) throw std::runtime_error("Unexpected EOF in sequence item");
                int child_ind = get_indent(lines[idx]);
                if (child_ind <= cur_ind) throw std::runtime_error("Missing indent for sequence block item");
                item = parse_node(idx, child_ind, anch);
            }
            if (it_anch) {
                NodeRef ref = std::make_shared<Node>(std::move(item));
                ref->anchor = *it_anch;
                anch[*it_anch] = ref;
                item = Node(ref);
            }
            seq.push_back(std::move(item));
            if (on_same_line) idx++;
        }
        return Node(std::move(seq));
    };

    parse_mapping = [&lines, &skip_empty_lines, &get_content, &get_indent, &trim, &trim_left, &parse_scalar, &parse_node](size_t& idx, int cur_ind, std::map<std::string, NodeRef>& anch) -> Node {
        Mapping mapp;
        while (idx < lines.size()) {
            skip_empty_lines(idx);
            if (idx >= lines.size()) break;
            int th_ind = get_indent(lines[idx]);
            if (th_ind < cur_ind) break;
            if (th_ind > cur_ind) throw std::runtime_error("Indent error in mapping");
            std::string cnt = get_content(lines[idx]);
            if (cnt.empty()) { idx++; continue; }
            size_t col = cnt.find(':');
            if (col == std::string::npos) break;
            std::string ky = trim(cnt.substr(0, col));
            std::string val_cnt = trim_left(cnt.substr(col + 1));
            std::optional<std::string> val_anch;
            if (val_cnt.compare(0, 1, "&") == 0) {
                size_t sp = val_cnt.find(' ');
                if (sp == std::string::npos) sp = val_cnt.size();
                val_anch = val_cnt.substr(1, sp - 1);
                val_cnt = trim_left(val_cnt.substr(sp));
            }
            Node val;
            bool on_same_line = !val_cnt.empty();
            if (val_cnt.compare(0, 1, "*") == 0) {
                std::string al = val_cnt.substr(1);
                auto it = anch.find(al);
                if (it == anch.end()) throw std::runtime_error("Unknown alias: " + al);
                val = Node(it->second);
            } else if (on_same_line) {
                val = parse_scalar(val_cnt);
            } else {
                idx++;
                skip_empty_lines(idx);
                if (idx >= lines.size()) {
                    val = Node(nullptr);
                } else {
                    int child_ind = get_indent(lines[idx]);
                    if (child_ind <= cur_ind) {
                        val = Node(nullptr);
                        idx--;  // rewind to process next entry
                    } else {
                        val = parse_node(idx, child_ind, anch);
                    }
                }
            }
            if (val_anch) {
                NodeRef ref = std::make_shared<Node>(std::move(val));
                ref->anchor = *val_anch;
                anch[*val_anch] = ref;
                val = Node(ref);
            }
            mapp[ky] = std::move(val);
            if (on_same_line) idx++;
        }
        return Node(std::move(mapp));
    };

    parse_node = [&lines, &skip_empty_lines, &get_indent, &get_content, &trim_left, &parse_scalar, &parse_sequence, &parse_mapping](size_t& idx, int cur_ind, std::map<std::string, NodeRef>& anch) -> Node {
        skip_empty_lines(idx);
        if (idx >= lines.size()) throw std::runtime_error("Unexpected EOF");
        int th_ind = get_indent(lines[idx]);
        if (th_ind != cur_ind) throw std::runtime_error("Indentation mismatch: expected " + std::to_string(cur_ind) + ", got " + std::to_string(th_ind));
        std::string cnt = get_content(lines[idx]);
        std::optional<std::string> anch_opt;
        std::string val_cnt;
        if (cnt.compare(0, 1, "&") == 0) {
            size_t sp = cnt.find(' ');
            if (sp == std::string::npos) sp = cnt.size();
            anch_opt = cnt.substr(1, sp - 1);
            val_cnt = trim_left(cnt.substr(sp));
        } else {
            val_cnt = cnt;
        }
        if (val_cnt.compare(0, 1, "*") == 0) {
            std::string al = val_cnt.substr(1);
            auto it = anch.find(al);
            if (it == anch.end()) throw std::runtime_error("Unknown alias: " + al);
            idx++;
            return Node(it->second);
        }
        bool is_block = val_cnt.empty();
        int ch_ind = -1;
        size_t peek_idx = idx;
        std::string peek_cnt = val_cnt;
        if (is_block) {
            peek_idx++;
            skip_empty_lines(peek_idx);
            if (peek_idx >= lines.size() || get_indent(lines[peek_idx]) <= cur_ind) {
                is_block = false;
                peek_cnt = "";
            } else {
                ch_ind = get_indent(lines[peek_idx]);
                peek_cnt = get_content(lines[peek_idx]);
            }
        }
        if (is_block) idx = peek_idx;
        bool seq = peek_cnt.compare(0, 1, "-") == 0;
        bool mapp = !seq && peek_cnt.find(':') != std::string::npos;
        Node n;
        if (seq) {
            n = parse_sequence(idx, is_block ? ch_ind : cur_ind, anch);
        } else if (mapp) {
            n = parse_mapping(idx, is_block ? ch_ind : cur_ind, anch);
        } else {
            n = parse_scalar(val_cnt);
            idx++;
        }
        if (anch_opt) {
            NodeRef ref = std::make_shared<Node>(std::move(n));
            ref->anchor = *anch_opt;
            anch[*anch_opt] = ref;
            n = Node(ref);
        }
        return n;
    };

    std::map<std::string, NodeRef> anchors;
    size_t idx = 0;
    Node root = parse_node(idx, 0, anchors);
    skip_empty_lines(idx);
    if (idx < lines.size()) throw std::runtime_error("Extra content after root node");
    return root;
}

} // namespace yamln