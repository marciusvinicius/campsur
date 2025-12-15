#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace criogenio {
using Variant = std::variant<int, float, bool, std::string>;

struct SerializedComponent {
  std::string type;
  std::unordered_map<std::string, Variant> fields;
};

struct SerializedEntity {
  int id;
  std::string name;
  std::vector<SerializedComponent> components;
};

struct SerializedWorld {
  std::string name;
  std::vector<SerializedEntity> entities;
};
} // namespace criogenio
