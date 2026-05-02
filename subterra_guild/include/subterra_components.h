#pragma once

#include "components.h"
#include "serialization.h"
#include <array>
#include <string>

namespace subterra {

using criogenio::SerializedComponent;

/** Only the controllable player should have this (camera, bounds, pickups). */
class PlayerTag : public criogenio::Component {
public:
  std::string TypeName() const override { return "PlayerTag"; }
  SerializedComponent Serialize() const override { return {"PlayerTag", {}}; }
  void Deserialize(const SerializedComponent &) override {}
};

/** Spawned enemies / map mobs; cleared on map load. */
class MobTag : public criogenio::Component {
public:
  std::string TypeName() const override { return "MobTag"; }
  SerializedComponent Serialize() const override { return {"MobTag", {}}; }
  void Deserialize(const SerializedComponent &) override {}
};

/** Dropped item in the world; collected on overlap with player. */
class WorldPickup : public criogenio::Component {
public:
  std::string item_id;
  int count = 1;
  float width = 28.f;
  float height = 28.f;

  WorldPickup() = default;
  WorldPickup(std::string id, int c, float w = 28.f, float h = 28.f)
      : item_id(std::move(id)), count(c), width(w), height(h) {}

  std::string TypeName() const override { return "WorldPickup"; }
  SerializedComponent Serialize() const override {
    return {"WorldPickup",
            {{"item_id", item_id},
             {"count", count},
             {"width", width},
             {"height", height}}};
  }
  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("item_id"); it != data.fields.end())
      item_id = criogenio::GetString(it->second);
    if (auto it = data.fields.find("count"); it != data.fields.end())
      count = criogenio::GetInt(it->second);
    if (auto it = data.fields.find("width"); it != data.fields.end())
      width = criogenio::GetFloat(it->second);
    if (auto it = data.fields.find("height"); it != data.fields.end())
      height = criogenio::GetFloat(it->second);
  }
};

struct InventorySlot {
  std::string item_id;
  int count = 0;
};

class Inventory : public criogenio::Component {
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

  /** @return Amount that did not fit. */
  int TryAdd(const std::string &item_id, int count);

  /** @return Amount actually removed. */
  int TryRemove(const std::string &item_id, int count);

  void Clear();

  std::string FormatLine() const;

  const std::array<InventorySlot, kNumSlots> &Slots() const { return slots; }

private:
  std::array<InventorySlot, kNumSlots> slots{};
};

} // namespace subterra
