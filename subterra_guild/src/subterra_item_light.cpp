#include "subterra_item_light.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace subterra {

namespace {

std::unordered_map<std::string, ItemLightEmission> g_by_prefab;
std::unordered_map<std::string, std::vector<ItemEventDispatchDef>> g_dispatch_by_prefab;

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

int json_number_to_int(const nlohmann::json &j, int fallback = 0) {
  if (j.is_number_integer())
    return j.get<int>();
  if (j.is_number_float())
    return static_cast<int>(j.get<float>());
  return fallback;
}

/** Remove // comments outside JSON string literals. */
std::string stripLineCommentsOutsideStrings(const std::string &in) {
  std::string out;
  out.reserve(in.size());
  bool in_str = false;
  bool esc = false;
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (esc) {
      out += c;
      esc = false;
      continue;
    }
    if (in_str) {
      if (c == '\\')
        esc = true;
      else if (c == '"')
        in_str = false;
      out += c;
      continue;
    }
    if (c == '"') {
      in_str = true;
      out += c;
      continue;
    }
    if (c == '/' && i + 1 < in.size() && in[i + 1] == '/') {
      while (i < in.size() && in[i] != '\n')
        ++i;
      continue;
    }
    out += c;
  }
  return out;
}

} // namespace

namespace SubterraItemLight {

void Clear() {
  g_by_prefab.clear();
  g_dispatch_by_prefab.clear();
}

bool TryLoadFromPath(const std::string &path) {
  Clear();
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (raw.empty())
    return false;
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(stripLineCommentsOutsideStrings(raw));
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
    g_by_prefab[prefab] = em;
    if (el.contains("event_dispatch") && el["event_dispatch"].is_array()) {
      auto &defs = g_dispatch_by_prefab[prefab];
      defs.clear();
      for (const auto &dv : el["event_dispatch"]) {
        if (!dv.is_object())
          continue;
        if (!dv.contains("event") || !dv["event"].is_string())
          continue;
        ItemEventDispatchDef d;
        d.event = lowerAscii(dv["event"].get<std::string>());
        if (dv.contains("event_trigger_when") && dv["event_trigger_when"].is_string())
          d.event_trigger_when = lowerAscii(dv["event_trigger_when"].get<std::string>());
        if (dv.contains("event_action_get_data") && dv["event_action_get_data"].is_string())
          d.event_action_get_data = lowerAscii(dv["event_action_get_data"].get<std::string>());
        if (dv.contains("params") && dv["params"].is_object())
          d.params = dv["params"];
        if (dv.contains("cooldown_ms"))
          d.cooldown_ms = std::max(0, json_number_to_int(dv["cooldown_ms"], 0));
        defs.push_back(std::move(d));
      }
    }
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

bool TryGetEventDispatchDefs(std::string_view item_prefab_id,
                             const std::vector<ItemEventDispatchDef> *&out) {
  out = nullptr;
  std::string key = lowerAscii(std::string(item_prefab_id));
  auto it = g_dispatch_by_prefab.find(key);
  if (it == g_dispatch_by_prefab.end())
    return false;
  out = &it->second;
  return true;
}

} // namespace SubterraItemLight

} // namespace subterra
