#include "yamln_serialize.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <stdexcept>

namespace yamln {

std::string format_key(const std::string& key) {
    if (key.empty()) {
        std::ostringstream oss;
        oss << std::quoted(key);
        return oss.str();
    }

    char first = key[0];
    if (!std::isalpha(static_cast<unsigned char>(first)) && first != '_') {
        std::ostringstream oss;
        oss << std::quoted(key);
        return oss.str();
    }

    bool needs_quote = false;
    for (char c : key) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            needs_quote = true;
            break;
        }
    }

    if (needs_quote) {
        std::ostringstream oss;
        oss << std::quoted(key);
        return oss.str();
    }

    return key;
}

bool is_scalar(const Node& node) {
    return node.is_null() || node.is_bool() || node.is_number() || node.is_string() || node.is_alias();
}

void serialize_scalar(const Node& node, std::ostream& os) {
    if (node.is_alias()) {
        const Node& ref_node = node.as_alias();
        if (!ref_node.anchor) {
            throw std::runtime_error("Alias references a node without an anchor");
        }
        os << "*" << *ref_node.anchor;
        return;
    }

    if (node.is_null()) {
        os << "null";
    } else if (node.is_bool()) {
        os << (node.as_bool() ? "true" : "false");
    } else if (std::holds_alternative<int>(node.data)) {
        os << std::get<int>(node.data);
    } else if (std::holds_alternative<double>(node.data)) {
        os << std::get<double>(node.data);
    } else if (node.is_string()) {
        os << std::quoted(node.as_string());
    } else {
        throw std::runtime_error("Not a scalar node");
    }
}

void serialize_container(const Node& node, std::ostream& os, int indent) {
    if (node.is_sequence()) {
        const Sequence& seq = node.as_sequence();
        if (seq.empty()) {
            os << "[]";
            return;
        }

        bool first = true;
        for (const Node& item : seq) {
            if (!first) {
                os << "\n";
            }
            first = false;

            os << std::string(indent, ' ') << "-";
            std::string anch = item.anchor ? " &" + *item.anchor : "";
            os << anch;

            if (is_scalar(item)) {
                os << " ";
                serialize_scalar(item, os);
            } else {
                os << "\n";
                serialize_container(item, os, indent + 2);
            }
        }
    } else if (node.is_mapping()) {
        const Mapping& map = node.as_mapping();
        if (map.empty()) {
            os << "{}";
            return;
        }

        bool first = true;
        for (const auto& kv : map) {
            if (!first) {
                os << "\n";
            }
            first = false;

            os << std::string(indent, ' ') << format_key(kv.first) << ":";
            const Node& val = kv.second;
            std::string anch = val.anchor ? " &" + *val.anchor : "";
            os << anch;

            if (is_scalar(val)) {
                os << " ";
                serialize_scalar(val, os);
            } else {
                os << "\n";
                serialize_container(val, os, indent + 2);
            }
        }
    } else {
        throw std::runtime_error("Not a container node");
    }
}

std::string serialize(const Node& n) {
    std::ostringstream os;

    bool is_scalar_type = is_scalar(n);

    std::string anch = n.anchor ? "&" + *n.anchor : "";
    if (!anch.empty()) {
        os << anch;
        if (is_scalar_type) {
            os << " ";
        } else {
            os << "\n";
        }
    }

    if (is_scalar_type) {
        serialize_scalar(n, os);
    } else {
        serialize_container(n, os, 0);
    }

    return os.str();
}

} // namespace yamln
