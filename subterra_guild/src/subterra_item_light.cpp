#include "subterra_item_light.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

namespace subterra {

namespace {

std::unordered_map<std::string, ItemLightEmission> g_by_prefab;

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

float json_number_to_float(const nlohmann::json &j) {
  if (j.is_number_float())
    return j.get<float>();
  if (j.is_number_integer())
    return static_cast<float>(j.get<int>());
  return 0.f;
}

} // namespace

namespace SubterraItemLight {

void Clear() { g_by_prefab.clear(); }

bool TryLoadFromPath(const std::string &path) {
  Clear();
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  nlohmann::json root;
  try {
    f >> root;
  } catch (...) {
    Clear();
    return false;
  }
  if (!root.is_object() || !root.contains("list") || !root["list"].is_array()) {
    Clear();
    return false;
  }
  for (const auto &el : root["list"]) {
    if (!el.is_object() || !el.contains("light_emission"))
      continue;
    std::string prefab;
    if (el.contains("prefab_name") && el["prefab_name"].is_string())
      prefab = el["prefab_name"].get<std::string>();
    else if (el.contains("name") && el["name"].is_string()) {
      prefab = lowerAscii(el["name"].get<std::string>());
      for (char &c : prefab) {
        if (c == ' ')
          c = '_';
      }
    }
    prefab = lowerAscii(std::move(prefab));
    if (prefab.empty())
      continue;
    const auto &le = el["light_emission"];
    if (!le.is_object())
      continue;
    ItemLightEmission em;
    em.valid = true;
    if (le.contains("radius"))
      em.radius = std::max(8.f, json_number_to_float(le["radius"]));
    if (le.contains("intensity"))
      em.intensity = std::max(0.05f, json_number_to_float(le["intensity"]));
    if (le.contains("color") && le["color"].is_array() && le["color"].size() >= 3) {
      const auto &arr = le["color"];
      em.r = static_cast<unsigned char>(
          std::clamp(static_cast<int>(arr[0].is_number_integer() ? arr[0].get<int>()
                                                                  : static_cast<int>(arr[0].get<float>())),
                     0, 255));
      em.g = static_cast<unsigned char>(
          std::clamp(static_cast<int>(arr[1].is_number_integer() ? arr[1].get<int>()
                                                                  : static_cast<int>(arr[1].get<float>())),
                     0, 255));
      em.b = static_cast<unsigned char>(
          std::clamp(static_cast<int>(arr[2].is_number_integer() ? arr[2].get<int>()
                                                                  : static_cast<int>(arr[2].get<float>())),
                     0, 255));
    }
    em.lantern_style = (prefab == "lantern");
    g_by_prefab[std::move(prefab)] = em;
  }
  return !g_by_prefab.empty();
}

bool Lookup(const std::string &item_prefab_id, ItemLightEmission &out) {
  out = ItemLightEmission{};
  std::string key = lowerAscii(item_prefab_id);
  auto it = g_by_prefab.find(key);
  if (it == g_by_prefab.end())
    return false;
  out = it->second;
  return out.valid;
}

} // namespace SubterraItemLight

} // namespace subterra
