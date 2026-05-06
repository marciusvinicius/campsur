#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "subterra_item_light.h"

int main() {
  namespace fs = std::filesystem;
  const fs::path tempPath = fs::temp_directory_path() / "subterra_items_dispatch_test.json";
  std::ofstream out(tempPath);
  out << R"({
    "type":"entities",
    "list":[
      {
        "type":"item",
        "name":"Energy Torch",
        "prefab_name":"energy_torch",
        "light_emission":{"radius":128,"intensity":1.0,"color":[255,0,0]},
        "event_dispatch":[
          {
            "event":"light_emission_touched",
            "event_trigger_when":"on_collision_enter",
            "event_action_get_data":"energy_torch_activated",
            "cooldown_ms":300
          }
        ]
      }
    ]
  })";
  out.close();

  subterra::SubterraItemLight::Clear();
  assert(subterra::SubterraItemLight::TryLoadFromPath(tempPath.string()));

  const std::vector<subterra::ItemEventDispatchDef> *defs = nullptr;
  assert(subterra::SubterraItemLight::TryGetEventDispatchDefs("energy_torch", defs));
  assert(defs != nullptr);
  assert(defs->size() == 1);
  assert((*defs)[0].event == "light_emission_touched");
  assert((*defs)[0].event_trigger_when == "on_collision_enter");
  assert((*defs)[0].event_action_get_data == "energy_torch_activated");
  assert((*defs)[0].cooldown_ms == 300);

  fs::remove(tempPath);
  std::cout << "item_event_dispatch_test passed\n";
  return 0;
}
