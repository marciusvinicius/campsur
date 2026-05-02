#include "subterra_loadout.h"
#include "inventory.h"
#include "item_catalog.h"
#include <cctype>

namespace subterra {

namespace {

std::string lowerCopy(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool idEqualsFold(const std::string &a, const std::string &b) {
  return lowerCopy(a) == lowerCopy(b);
}

int equipIndexForKind(criogenio::ItemWearSlot k) {
  switch (k) {
  case criogenio::ItemWearSlot::MainHand:
    return 0;
  case criogenio::ItemWearSlot::OffHand:
    return 1;
  case criogenio::ItemWearSlot::Armor:
    return 2;
  default:
    return -1;
  }
}

} // namespace

criogenio::SerializedComponent SubterraLoadout::Serialize() const {
  criogenio::SerializedComponent out{"SubterraLoadout", {}};
  out.fields["active_action_slot"] = std::to_string(active_action_slot);
  for (int i = 0; i < kActionBarSlots; ++i) {
    if (!action_bar[static_cast<size_t>(i)].empty())
      out.fields["ab_" + std::to_string(i)] = action_bar[static_cast<size_t>(i)];
  }
  for (int i = 0; i < kEquipSlots; ++i) {
    if (!equipment[static_cast<size_t>(i)].empty())
      out.fields["eq_" + std::to_string(i)] = equipment[static_cast<size_t>(i)];
  }
  return out;
}

void SubterraLoadout::Deserialize(const criogenio::SerializedComponent &data) {
  action_bar.fill({});
  equipment.fill({});
  if (auto it = data.fields.find("active_action_slot"); it != data.fields.end())
    active_action_slot = criogenio::GetInt(it->second);
  for (int i = 0; i < kActionBarSlots; ++i) {
    auto k = "ab_" + std::to_string(i);
    if (auto it = data.fields.find(k); it != data.fields.end())
      action_bar[static_cast<size_t>(i)] = criogenio::GetString(it->second);
  }
  for (int i = 0; i < kEquipSlots; ++i) {
    auto k = "eq_" + std::to_string(i);
    if (auto it = data.fields.find(k); it != data.fields.end())
      equipment[static_cast<size_t>(i)] = criogenio::GetString(it->second);
  }
}

void SubterraLoadout::ClearActionBarRefsForItem(const std::string &item_id) {
  if (item_id.empty())
    return;
  for (auto &s : action_bar) {
    if (idEqualsFold(s, item_id))
      s.clear();
  }
}

void SubterraLoadout::ClearRefsForItem(const std::string &item_id) {
  ClearActionBarRefsForItem(item_id);
  if (item_id.empty())
    return;
  for (auto &s : equipment) {
    if (idEqualsFold(s, item_id))
      s.clear();
  }
}

bool EquipFromInventory(SubterraLoadout &load, criogenio::Inventory &inv,
                        criogenio::ItemWearSlot slotKind, const std::string &item_id,
                        std::string &errOut) {
  errOut.clear();
  if (item_id.empty()) {
    errOut = "no item";
    return false;
  }
  if (criogenio::ItemCatalog::WearSlotFor(item_id) != slotKind) {
    errOut = "wrong slot for item";
    return false;
  }
  int ei = equipIndexForKind(slotKind);
  if (ei < 0) {
    errOut = "invalid slot";
    return false;
  }
  if (inv.TryRemove(item_id, 1) < 1) {
    errOut = "not in inventory";
    return false;
  }
  std::string equippedId = item_id;
  if (const criogenio::ItemDef *def = criogenio::ItemCatalog::Find(item_id))
    equippedId = def->id;
  std::string prev = std::move(load.equipment[static_cast<size_t>(ei)]);
  load.equipment[static_cast<size_t>(ei)] = equippedId;
  if (!prev.empty()) {
    int overflow = inv.TryAdd(prev, 1);
    if (overflow > 0) {
      load.equipment[static_cast<size_t>(ei)] = std::move(prev);
      inv.TryAdd(item_id, 1);
      errOut = "inventory full";
      return false;
    }
  }
  // Do not call ClearRefsForItem here: it clears equipment slots matching the id and would
  // wipe the slot we just filled. Only drop duplicate hotbar entries (Odin-style).
  load.ClearActionBarRefsForItem(equippedId);
  return true;
}

bool UnequipToInventory(SubterraLoadout &load, criogenio::Inventory &inv, int equipIndex,
                        std::string &errOut) {
  errOut.clear();
  if (equipIndex < 0 || equipIndex >= kEquipSlots) {
    errOut = "bad index";
    return false;
  }
  std::string cur = load.equipment[static_cast<size_t>(equipIndex)];
  if (cur.empty())
    return true;
  int left = inv.TryAdd(cur, 1);
  if (left > 0) {
    errOut = "inventory full";
    return false;
  }
  load.equipment[static_cast<size_t>(equipIndex)].clear();
  return true;
}

void AssignActionBarSlot(SubterraLoadout &load, int slot, const std::string &item_id) {
  if (slot < 0 || slot >= kActionBarSlots)
    return;
  load.action_bar[static_cast<size_t>(slot)] = item_id;
}

} // namespace subterra
