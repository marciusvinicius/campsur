#include "subterra_item_consumable.h"
#include "subterra_json_strip.h"
#include "json.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace subterra {

namespace {

struct ConsumeDef {
  float health = 0.f;
  float food = 0.f;
};

std::unordered_map<std::string, ConsumeDef> g_by_prefab;

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

} // namespace

bool SubterraItemConsumableTryLoadFromPath(const std::string &path) {
  g_by_prefab.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(SubterraStripJsonLineComments(raw));
  } catch (...) {
    return false;
  }
  if (!root.is_object() || !root.contains("list") || !root["list"].is_array())
    return false;
  for (const auto &el : root["list"]) {
    if (!el.is_object())
      continue;
    std::string role;
    if (el.contains("inventory_role") && el["inventory_role"].is_string())
      role = lowerAscii(el["inventory_role"].get<std::string>());
    if (role != "consumable")
      continue;
    std::string prefab;
    if (el.contains("prefab_name") && el["prefab_name"].is_string())
      prefab = el["prefab_name"].get<std::string>();
    if (prefab.empty() && el.contains("name") && el["name"].is_string()) {
      prefab = lowerAscii(el["name"].get<std::string>());
      for (char &c : prefab) {
        if (c == ' ')
          c = '_';
      }
    }
    if (prefab.empty())
      continue;
    ConsumeDef d;
    if (el.contains("consume_restore_health")) {
      const auto &v = el["consume_restore_health"];
      if (v.is_number_float())
        d.health = v.get<float>();
      else if (v.is_number_integer())
        d.health = static_cast<float>(v.get<int>());
    }
    if (el.contains("consume_restore_food")) {
      const auto &v = el["consume_restore_food"];
      if (v.is_number_float())
        d.food = v.get<float>();
      else if (v.is_number_integer())
        d.food = static_cast<float>(v.get<int>());
    }
    if (d.health > 0.f || d.food > 0.f)
      g_by_prefab[lowerAscii(std::move(prefab))] = d;
  }
  return true;
}

bool SubterraItemConsumableLookup(const std::string &item_id, float &out_restore_health,
                                  float &out_restore_food) {
  out_restore_health = 0.f;
  out_restore_food = 0.f;
  std::string key = lowerAscii(item_id);
  auto it = g_by_prefab.find(key);
  if (it == g_by_prefab.end())
    return false;
  out_restore_health = it->second.health;
  out_restore_food = it->second.food;
  return out_restore_health > 0.f || out_restore_food > 0.f;
}

} // namespace subterra
