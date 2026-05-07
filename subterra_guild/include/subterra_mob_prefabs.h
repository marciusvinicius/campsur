#pragma once

#include "json.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace subterra {

struct SubterraMobEventListenerDef {
  std::string event;
  nlohmann::json required_data = nlohmann::json::object();
  std::string action;
};

struct SubterraMobPrefabDef {
  std::string prefab_name;
  std::string display_name;
  int size = 64;
  std::string texture_path;
  int texture_x = 0;
  int texture_y = 0;
  std::string animation_path;
  nlohmann::json default_entity_data = nlohmann::json::object();
  std::vector<SubterraMobEventListenerDef> event_listeners;
};

void SubterraMobPrefabsClear();
bool SubterraMobPrefabsTryLoadFromPath(const std::string &path);
/** Load mob entries from a single `.campsurmeta` file without mutating the global registry. */
bool SubterraMobPrefabsReadListFromPath(const std::string &path,
                                        std::vector<SubterraMobPrefabDef> &out);
bool SubterraMobPrefabNameIsRegistered(std::string_view prefabName);
bool SubterraMobTryGetPrefabDef(std::string_view prefabName, SubterraMobPrefabDef &out);

} // namespace subterra
