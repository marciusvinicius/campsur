#pragma once

#include <string>
#include <string_view>

namespace subterra {

/** Optional `rest_emission` from entities_interactable.json (reference `EntityPrefab`). */
struct SubterraInteractableRestDef {
  float rest_radius = 0.f;
  float stamina_per_sec = 0.f;
  float health_per_sec = 0.f;
  float food_per_sec = 0.f;
};

/**
 * Loads `prefab_name` values from entities_interactable.json (same layout as Odin reference).
 * Strips // comments so the shipped JSON5-style file parses. Call from startup; safe to call
 * multiple times (replaces the set).
 */
bool SubterraInteractablePrefabsTryLoadFromPath(const std::string &path);

/** True if this id is an interactable prefab (door, chest, campfire, …), not a world pickup. */
bool SubterraInteractablePrefabNameIsRegistered(std::string_view prefabName);

void SubterraInteractablePrefabsClear();

bool SubterraInteractableTryGetRest(std::string_view interactable_type_normalized,
                                    SubterraInteractableRestDef &out);

} // namespace subterra
