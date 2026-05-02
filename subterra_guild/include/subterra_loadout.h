#pragma once

#include "components.h"
#include "item_catalog.h"
#include "serialization.h"
#include <array>
#include <string>

namespace criogenio {
class Inventory;
}

namespace subterra {

static constexpr int kActionBarSlots = 5;
static constexpr int kEquipSlots = 3;

/** R / L / body equipment + action bar (Subterra Guild–style). */
class SubterraLoadout : public criogenio::Component {
public:
  /** Selected action bar index 0..4, or -1 if none. */
  int active_action_slot = 0;
  std::array<std::string, kActionBarSlots> action_bar;
  std::array<std::string, kEquipSlots> equipment;

  std::string TypeName() const override { return "SubterraLoadout"; }
  criogenio::SerializedComponent Serialize() const override;
  void Deserialize(const criogenio::SerializedComponent &data) override;

  /** Remove references to `item_id` from bar and equipment (case-insensitive). */
  void ClearRefsForItem(const std::string &item_id);
  /** Action bar only — safe to call after moving an item into an equipment slot. */
  void ClearActionBarRefsForItem(const std::string &item_id);
};

bool EquipFromInventory(SubterraLoadout &load, criogenio::Inventory &inv,
                        criogenio::ItemWearSlot slotKind, const std::string &item_id,
                        std::string &errOut);

bool UnequipToInventory(SubterraLoadout &load, criogenio::Inventory &inv, int equipIndex,
                        std::string &errOut);

void AssignActionBarSlot(SubterraLoadout &load, int slot, const std::string &item_id);

} // namespace subterra
