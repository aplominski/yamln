#pragma once

#include <string>
#include <iostream>

#include "../../include/yamln.h"

namespace yamln {

std::string format_key(const std::string& key);
bool is_scalar(const Node& node);
void serialize_scalar(const Node& node, std::ostream& os);
void serialize_container(const Node& node, std::ostream& os, int indent);

std::string serialize(const Node& n);

} // namespace yamln
