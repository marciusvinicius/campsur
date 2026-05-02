#pragma once

#include <string>
#include <utility>
#include <vector>

namespace criogenio {

/**
 * Preferred equipment slot for an item (Subterra-style `equip_slot`).
 * Names avoid `Body` / `RightHand` which some platform headers `#define`.
 */
enum class ItemWearSlot { None = 0, MainHand, OffHand, Armor };

struct ItemDef {
  const char *id;
  const char *display_name;
  int max_stack;
  ItemWearSlot wear_slot = ItemWearSlot::None;
  /** If true, "Use" consumes one from inventory (e.g. potions). */
  bool consumable = false;
};

namespace ItemCatalog {

const ItemDef *Find(const std::string &id);
int MaxStack(const std::string &id);
std::string DisplayName(const std::string &id);
ItemWearSlot WearSlotFor(const std::string &id);
bool IsConsumable(const std::string &id);
void ListBuiltin(std::vector<std::pair<std::string, std::string>> &out);

} // namespace ItemCatalog

} // namespace criogenio
