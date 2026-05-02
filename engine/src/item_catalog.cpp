#include "item_catalog.h"

namespace criogenio {

namespace {

const ItemDef kBuiltin[] = {
    {"wood", "Wood", 99, ItemWearSlot::None, false},
    {"stone", "Stone", 99, ItemWearSlot::None, false},
    {"ore", "Ore", 64, ItemWearSlot::Armor, false},
    {"potion", "Health potion", 10, ItemWearSlot::None, true},
    {"key", "Key", 1, ItemWearSlot::OffHand, false},
    {"coin", "Coin", 9999, ItemWearSlot::None, false},
    {"torch", "Torch", 99, ItemWearSlot::MainHand, false},
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

void ListBuiltin(std::vector<std::pair<std::string, std::string>> &out) {
  out.clear();
  for (const auto &it : kBuiltin)
    out.emplace_back(it.id, it.display_name);
}

ItemWearSlot WearSlotFor(const std::string &id) {
  const ItemDef *d = Find(id);
  return d ? d->wear_slot : ItemWearSlot::None;
}

bool IsConsumable(const std::string &id) {
  const ItemDef *d = Find(id);
  return d && d->consumable;
}

} // namespace ItemCatalog

} // namespace criogenio
