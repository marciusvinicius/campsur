#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "subterra_mob_prefabs.h"

int main() {
  namespace fs = std::filesystem;
  const fs::path tempPath = fs::temp_directory_path() / "subterra_mob_prefabs_test.json";
  std::ofstream out(tempPath);
  out << R"({
    "type":"entities",
    "list":[
      {
        "type":"enemy",
        "name":"Zombie",
        "prefab_name":"zombie",
        "size":64,
        "animation_path":"data/animations/mob1.json",
        "event_listeners":[
          {
            "event":"light_emission_touched",
            "required_data":{"light_color":[255,0,0]},
            "action":"show_hide_enemy_on_light_range"
          }
        ],
        "entity_data":{"brain_type":"simple_chase_player","hidden":true}
      }
    ]
  })";
  out.close();

  subterra::SubterraMobPrefabsClear();
  assert(subterra::SubterraMobPrefabsTryLoadFromPath(tempPath.string()));
  assert(subterra::SubterraMobPrefabNameIsRegistered("zombie"));

  subterra::SubterraMobPrefabDef def{};
  assert(subterra::SubterraMobTryGetPrefabDef("zombie", def));
  assert(def.prefab_name == "zombie");
  assert(def.size == 64);
  assert(def.default_entity_data.is_object());
  assert(def.default_entity_data["hidden"].is_boolean());
  assert(def.default_entity_data["hidden"].get<bool>());
  assert(def.event_listeners.size() == 1);
  assert(def.event_listeners[0].event == "light_emission_touched");
  assert(def.event_listeners[0].action == "show_hide_enemy_on_light_range");

  fs::remove(tempPath);
  std::cout << "mob_prefabs_test passed\n";
  return 0;
}
