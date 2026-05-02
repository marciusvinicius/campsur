#pragma once

#include <string>

namespace subterra {

/** `light_emission` from `entities_items.json` (per `prefab_name`). */
struct ItemLightEmission {
  bool valid = false;
  /** World-space radius in pixels (Tiled/prefab units), scaled by camera zoom when drawing. */
  float radius = 128.f;
  float intensity = 1.f;
  unsigned char r = 255, g = 180, b = 80;
  /** Lantern follows reference day/night rules; other emitters use torch-style visibility. */
  bool lantern_style = false;
};

namespace SubterraItemLight {

void Clear();
/** Parse `entities_items.json` list[].prefab_name + light_emission. Returns false if unreadable. */
bool TryLoadFromPath(const std::string &path);
bool Lookup(const std::string &item_prefab_id, ItemLightEmission &out);

} // namespace SubterraItemLight

} // namespace subterra
