#pragma once

#include <string>

namespace subterra {

/** Authoritative world tuning (aligned with reference `WorldRules` + server world_config.json). */
struct SubterraWorldRules {
  float health_max = 100.f;
  float stamina_max = 100.f;
  float food_max = 100.f;
  float health_regen_rate = 1.5f;
  float stamina_regen_rate = 8.f;
  float food_consumption_rate = 0.055f;
  float health_consumption_rate = 4.f;
  float stamina_consumption_rate = 40.f;
  float food_regen_rate = 0.f;
};

bool SubterraWorldRulesTryLoadFromPath(const std::string &path, SubterraWorldRules &out);

} // namespace subterra
