#pragma once

#include <string>

namespace subterra {

struct ItemDef {
  const char *id;
  const char *display_name;
  int max_stack;
};

namespace ItemCatalog {

const ItemDef *Find(const std::string &id);
int MaxStack(const std::string &id);
std::string DisplayName(const std::string &id);

} // namespace ItemCatalog

} // namespace subterra
