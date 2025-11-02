/*
    yamln - minimalist C++ library for yaml
    Copyright (c) 2025 Aleksander Płomiński

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    3. Neither the name of the author nor the names of its contributors may be
       used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <stdexcept>
#include <optional>
#include <memory>

namespace yamln {

// Forward declaration
struct Node;

// Type aliases
using Mapping = std::map<std::string, Node>;
using Sequence = std::vector<Node>;

// YAML supports anchors (&name) and aliases (*name).
// We'll represent them as shared pointers, so that aliases can reference the same data.
using NodeRef = std::shared_ptr<Node>;

// Variant type representing YAML node content
using NodeType = std::variant<
    std::nullptr_t,
    bool,
    int,
    double,
    std::string,
    Sequence,
    Mapping,
    NodeRef // ← alias / anchor reference
>;

// Structure representing a YAML node
struct Node {
    NodeType data;
    std::optional<std::string> anchor; // holds &anchor name if present

    Node() : data(nullptr) {}
    Node(std::nullptr_t) : data(nullptr) {}
    Node(bool b) : data(b) {}
    Node(int i) : data(i) {}
    Node(double d) : data(d) {}
    Node(const std::string& s) : data(s) {}
    Node(const char* s) : data(std::string(s)) {}
    Node(const Sequence& s) : data(s) {}
    Node(const Mapping& m) : data(m) {}
    Node(const NodeRef& ref) : data(ref) {}

    // Type checks
    bool is_mapping() const { return std::holds_alternative<Mapping>(data); }
    bool is_sequence() const { return std::holds_alternative<Sequence>(data); }
    bool is_string()  const { return std::holds_alternative<std::string>(data); }
    bool is_number()  const { return std::holds_alternative<double>(data) || std::holds_alternative<int>(data); }
    bool is_bool()    const { return std::holds_alternative<bool>(data); }
    bool is_null()    const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_alias()   const { return std::holds_alternative<NodeRef>(data); }

    // Getters
    const Mapping& as_mapping() const { return std::get<Mapping>(data); }
    const Sequence& as_sequence() const { return std::get<Sequence>(data); }
    const std::string& as_string() const { return std::get<std::string>(data); }
    double as_number() const { 
        if (std::holds_alternative<int>(data)) return static_cast<double>(std::get<int>(data));
        return std::get<double>(data);
    }
    bool as_bool() const { return std::get<bool>(data); }
    Node& as_alias() { return *std::get<NodeRef>(data); }
    const Node& as_alias() const { return *std::get<NodeRef>(data); }

    // Object access
    Node& operator[](const std::string& key) {
        if (!is_mapping()) data = Mapping{};
        return std::get<Mapping>(data)[key];
    }

    const Node& operator[](const std::string& key) const {
        if (!is_mapping()) throw std::runtime_error("Node is not a mapping");
        return std::get<Mapping>(data).at(key);
    }

    // Sequence access
    Node& operator[](size_t index) {
        if (!is_sequence()) data = Sequence{};
        auto& seq = std::get<Sequence>(data);
        if (index >= seq.size()) seq.resize(index + 1);
        return seq[index];
    }

    const Node& operator[](size_t index) const {
        if (!is_sequence()) throw std::runtime_error("Node is not a sequence");
        return std::get<Sequence>(data).at(index);
    }

    // Comparison
    bool operator==(const Node& other) const { return data == other.data; }
    bool operator!=(const Node& other) const { return !(*this == other); }
};

// Public API
__attribute__((visibility("default"))) std::string serialize(const Node& n);
__attribute__((visibility("default"))) Node parse(const std::string& yaml);

} // namespace yamln
