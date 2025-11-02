# yamln - Minimalist C++ Library for YAML

## Overview

yamln is a lightweight C++ library for parsing and serializing YAML data. It provides a simple and intuitive API for working with YAML structures, supporting essential features like mappings, sequences, scalars, anchors, and aliases. Designed for minimalism, it has no external dependencies beyond the C++ standard library.

## Features

- **Parsing and Serialization**: Easily parse YAML strings into a `Node` structure and serialize `Node` objects back to YAML strings.
- **Node Types**: Supports all core YAML types including null, booleans, integers, doubles, strings, sequences (arrays), and mappings (objects).
- **Anchors and Aliases**: Handles YAML anchors (`&name`) and aliases (`*name`) using shared pointers for efficient reference sharing.
- **Type-Safe Access**: Provides type checking methods (e.g., `is_mapping()`, `is_sequence()`) and getters (e.g., `as_mapping()`, `as_string()`) with runtime error handling for invalid accesses.
- **Operator Overloads**: Convenient access to mappings and sequences using `[]` operators.
- **Standards Compliance**: Built with C++17 features like `std::variant` and `std::optional`.

## Usage

### Basic Example

```cpp
#include <iostream>
#include <yamln.h>

int main() {
    std::string yaml_str = R"(
        name: John Doe
        age: 30
        hobbies:
          - reading
          - coding
        address:
          street: 123 Main St
          city: Anytown
    )";

    // Parse YAML
    yamln::Node root = yamln::parse(yaml_str);

    // Access data
    std::cout << "Name: " << root["name"].as_string() << std::endl;
    std::cout << "Age: " << root["age"].as_number() << std::endl;
    std::cout << "First Hobby: " << root["hobbies"][0].as_string() << std::endl;

    // Modify and serialize
    root["age"] = 31;
    std::string serialized = yamln::serialize(root);
    std::cout << "Updated YAML:\n" << serialized << std::endl;

    return 0;
}
```

### Creating Nodes Programmatically

```cpp
yamln::Node node;
node["key"] = "value";
node["numbers"] = yamln::Sequence{1, 2, 3};
node["flag"] = true;

std::string yaml = yamln::serialize(node);
// Output:
// flag: true
// key: value
// numbers:
//   - 1
//   - 2
//   - 3
```

### Handling Anchors and Aliases

yamln supports YAML anchors and aliases natively. When parsing, aliases reference the same underlying data via shared pointers.

Example YAML input:
```
shared: &anchor
  foo: bar
ref: *anchor
```

Parsed Node:
- `root["shared"]` and `root["ref"]` will point to the same data.

## API Reference

### Key Classes and Types

- **`Node`**: Core structure representing a YAML node.
  - Constructors for various types (null, bool, int, double, string, sequence, mapping, reference).
  - Type checks: `is_mapping()`, `is_sequence()`, `is_string()`, `is_number()`, `is_bool()`, `is_null()`, `is_alias()`.
  - Getters: `as_mapping()`, `as_sequence()`, `as_string()`, `as_number()`, `as_bool()`, `as_alias()`.
  - Operators: `[]` for mapping (string key) and sequence (size_t index) access.

- **`Mapping`**: `std::map<std::string, Node>` for object-like structures.
- **`Sequence`**: `std::vector<Node>` for array-like structures.
- **`NodeRef`**: `std::shared_ptr<Node>` for anchors/aliases.

### Public Functions

- **`std::string serialize(const Node& n)`**: Converts a `Node` to a YAML string.
- **`Node parse(const std::string& yaml)`**: Parses a YAML string into a `Node`.

## Limitations

- Minimalist design means no advanced features like custom emitters, event-based parsing, or schema validation.
- Error handling is basic (throws `std::runtime_error` on parse failures or type mismatches).
- Does not support YAML 1.2 features like binary data or custom tags.

## Contributing

Contributions are welcome! Feel free to submit issues or pull requests on the repository.

## License

yamln is released under a BSD-like license. See the header file for full details.

Copyright (c) 2025 Aleksander Płomiński