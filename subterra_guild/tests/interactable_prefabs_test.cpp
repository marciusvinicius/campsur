#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "subterra_interactable_prefabs.h"

int main() {
  namespace fs = std::filesystem;
  const fs::path tempPath = fs::temp_directory_path() / "subterra_interactable_prefabs_test.json";
  std::ofstream out(tempPath);
  out << R"({
    "type":"entities",
    "list":[
      {
        "type":"interactable_event_listener",
        "prefab_name":"light_door",
        "entity_data":{"open":false,"locked":true},
        "event_listeners":[
          {
            "event":"light_emission_touched",
            "required_data":{"light_color":[255,0,0]},
            "action":"unlock_door"
          }
        ]
      }
    ]
  })";
  out.close();

  subterra::SubterraInteractablePrefabsClear();
  assert(subterra::SubterraInteractablePrefabsTryLoadFromPath(tempPath.string()));
  assert(subterra::SubterraInteractablePrefabNameIsRegistered("light_door"));
  assert(!subterra::SubterraInteractableTypeCanDirectUse("light_door"));

  subterra::SubterraInteractablePrefabDef def{};
  assert(subterra::SubterraInteractableTryGetPrefabDef("light_door", def));
  assert(def.is_event_listener_only);
  assert(def.default_entity_data.is_object());
  assert(def.default_entity_data["locked"].is_boolean());
  assert(def.default_entity_data["locked"].get<bool>());
  assert(def.event_listeners.size() == 1);
  assert(def.event_listeners[0].event == "light_emission_touched");
  assert(def.event_listeners[0].action == "unlock_door");

  fs::remove(tempPath);
  std::cout << "interactable_prefabs_test passed\n";
  return 0;
}
