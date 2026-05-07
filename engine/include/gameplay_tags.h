#pragma once

#include "components.h"
#include "serialization.h"
#include <string>

namespace criogenio {

/** Marks the controllable player (camera follow, bounds, pickups in Subterra). */
class PlayerTag : public Component {
public:
  std::string TypeName() const override { return "PlayerTag"; }
  SerializedComponent Serialize() const override { return {"PlayerTag", {}}; }
  void Deserialize(const SerializedComponent &) override {}
};

/** Spawned enemies / map mobs (cleared on map load in Subterra). */
class MobTag : public Component {
public:
  std::string TypeName() const override { return "MobTag"; }
  SerializedComponent Serialize() const override { return {"MobTag", {}}; }
  void Deserialize(const SerializedComponent &) override {}
};

/** World-space pickup (item id + count + hitbox). */
class WorldPickup : public Component {
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
      item_id = GetString(it->second);
    if (auto it = data.fields.find("count"); it != data.fields.end())
      count = GetInt(it->second);
    if (auto it = data.fields.find("width"); it != data.fields.end())
      width = GetFloat(it->second);
    if (auto it = data.fields.find("height"); it != data.fields.end())
      height = GetFloat(it->second);
  }
};

/** Editor/game traceability: entity was spawned from a `.campsurmeta` entry. */
class PrefabInstance : public Component {
public:
  std::string source_path;
  std::string prefab_name;

  PrefabInstance() = default;
  PrefabInstance(std::string src, std::string pfn)
      : source_path(std::move(src)), prefab_name(std::move(pfn)) {}

  std::string TypeName() const override { return "PrefabInstance"; }
  SerializedComponent Serialize() const override {
    return {"PrefabInstance",
            {{"source_path", source_path}, {"prefab_name", prefab_name}}};
  }
  void Deserialize(const SerializedComponent &data) override {
    if (auto it = data.fields.find("source_path"); it != data.fields.end())
      source_path = GetString(it->second);
    if (auto it = data.fields.find("prefab_name"); it != data.fields.end())
      prefab_name = GetString(it->second);
  }
};

} // namespace criogenio
