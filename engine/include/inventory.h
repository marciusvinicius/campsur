#pragma once

#include "components.h"
#include "serialization.h"
#include <array>
#include <string>

namespace criogenio {

struct InventorySlot {
  std::string item_id;
  int count = 0;
};

class Inventory : public Component {
public:
  static constexpr int kNumSlots = 24;

  std::string TypeName() const override { return "Inventory"; }

  SerializedComponent Serialize() const override {
    SerializedComponent out{"Inventory", {}};
    for (int i = 0; i < kNumSlots; ++i) {
      const auto &s = slots[static_cast<size_t>(i)];
      if (!s.item_id.empty() && s.count > 0)
        out.fields["slot_" + std::to_string(i)] = s.item_id + " " + std::to_string(s.count);
    }
    return out;
  }

  void Deserialize(const SerializedComponent &data) override;

  int TryAdd(const std::string &item_id, int count);
  int TryRemove(const std::string &item_id, int count);
  /** Like `TryRemove` but matches `item_id` to slots case-insensitively (catalog vs pickup ids). */
  int TryRemoveFolded(const std::string &item_id, int count);
  void Clear();
  std::string FormatLine() const;
  const std::array<InventorySlot, kNumSlots> &Slots() const { return slots; }

private:
  std::array<InventorySlot, kNumSlots> slots{};
};

} // namespace criogenio
