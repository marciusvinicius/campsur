#include "items.h"

namespace subterra {

namespace {

const ItemDef kBuiltin[] = {
    {"wood", "Wood", 99},
    {"stone", "Stone", 99},
    {"ore", "Ore", 64},
    {"potion", "Health potion", 10},
    {"key", "Key", 1},
    {"coin", "Coin", 9999},
};

} // namespace

namespace ItemCatalog {

const ItemDef *Find(const std::string &id) {
  std::string lower = id;
  for (char &c : lower)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (const auto &it : kBuiltin) {
    std::string bid = it.id;
    for (char &c : bid)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (bid == lower)
      return &it;
  }
  return nullptr;
}

int MaxStack(const std::string &id) {
  const ItemDef *d = Find(id);
  return d ? d->max_stack : 99;
}

std::string DisplayName(const std::string &id) {
  const ItemDef *d = Find(id);
  return d ? std::string(d->display_name) : id;
}

} // namespace ItemCatalog

} // namespace subterra
