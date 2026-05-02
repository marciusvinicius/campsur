#include "subterra_world_rules.h"
#include "subterra_json_strip.h"
#include "json.hpp"

#include <fstream>
#include <iterator>

namespace subterra {

bool SubterraWorldRulesTryLoadFromPath(const std::string &path, SubterraWorldRules &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (raw.empty())
    return false;
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(SubterraStripJsonLineComments(raw));
  } catch (...) {
    return false;
  }
  if (!root.is_object() || !root.contains("world") || !root["world"].is_object())
    return false;
  const auto &w = root["world"];
  auto getF = [&w](const char *key, float fallback) -> float {
    if (!w.contains(key))
      return fallback;
    const auto &v = w[key];
    if (v.is_number_float())
      return v.get<float>();
    if (v.is_number_integer())
      return static_cast<float>(v.get<int>());
    return fallback;
  };
  out.health_max = getF("health_satiety_max", out.health_max);
  out.stamina_max = getF("stamina_satiety_max", out.stamina_max);
  out.food_max = getF("food_satiety_max", out.food_max);
  out.food_consumption_rate = getF("food_depletion_rate", out.food_consumption_rate);
  out.health_consumption_rate = getF("health_satiety_depletion_rate", out.health_consumption_rate);
  out.stamina_consumption_rate = getF("stamina_depletion_on_run_rate", out.stamina_consumption_rate);
  out.health_regen_rate = getF("health_satiety_regen_rate", out.health_regen_rate);
  out.stamina_regen_rate = getF("stamina_satiety_regen_rate", out.stamina_regen_rate);
  out.food_regen_rate = getF("food_satiety_regen_rate", out.food_regen_rate);
  return true;
}

} // namespace subterra
