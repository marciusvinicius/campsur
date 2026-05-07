#pragma once

#include "components.h"
#include "serialization.h"
#include <string>

namespace criogenio {

/**
 * ECS-authored map overlap trigger (same semantics as Tiled objects used by Subterra
 * `BuildMapEventTriggers`). Position is the entity `Transform` (top-left of box).
 */
struct MapEventZone2D : public Component {
  std::string storage_key;
  float width = 32.f;
  float height = 32.f;
  bool point = false;
  std::string event_id;
  std::string event_trigger;
  std::string event_type;
  std::string object_type;
  std::string teleport_to;
  std::string target_level;
  std::string spawn_point;
  int spawn_count = 0;
  int monster_count = 0;
  std::string gameplay_actions;
  bool activated = true;

  std::string TypeName() const override { return "MapEventZone2D"; }
  SerializedComponent Serialize() const override;
  void Deserialize(const SerializedComponent &data) override;
};

/** Interactable volume; `Transform` is top-left; optional JSON object string for Tiled-like props. */
struct InteractableZone2D : public Component {
  float width = 32.f;
  float height = 32.f;
  bool point = false;
  int tiled_object_id = 0;
  std::string interactable_type;
  /** JSON object, e.g. {"opens_door":"id"} — merged into runtime `TmxProperty` list. */
  std::string properties_json;

  std::string TypeName() const override { return "InteractableZone2D"; }
  SerializedComponent Serialize() const override;
  void Deserialize(const SerializedComponent &data) override;
};

/** World pickup / mob spawn marker; `Transform` is top-left of area (center spawn like Tiled). */
struct WorldSpawnPrefab2D : public Component {
  std::string prefab_name;
  int quantity = 1;
  float width = 0.f;
  float height = 0.f;

  std::string TypeName() const override { return "WorldSpawnPrefab2D"; }
  SerializedComponent Serialize() const override;
  void Deserialize(const SerializedComponent &data) override;
};

} // namespace criogenio
