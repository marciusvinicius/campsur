#include "subterra_interactable_prefabs.h"
#include "json.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace subterra {

namespace {

std::unordered_set<std::string> g_interactablePrefabIds;
std::unordered_map<std::string, SubterraInteractableRestDef> g_interactableRest;

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string lowerAsciiView(std::string_view v) {
  std::string s(v.begin(), v.end());
  return lowerAscii(std::move(s));
}

/** Remove // comments outside of JSON string literals (reference JSON uses //). */
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

bool typeFieldIsInteractable(const nlohmann::json &el) {
  if (!el.is_object() || !el.contains("type"))
    return false;
  const auto &t = el["type"];
  if (!t.is_string())
    return false;
  std::string v = lowerAscii(t.get<std::string>());
  return v == "interactable";
}

} // namespace

void SubterraInteractablePrefabsClear() {
  g_interactablePrefabIds.clear();
  g_interactableRest.clear();
}

bool SubterraInteractablePrefabsTryLoadFromPath(const std::string &path) {
  g_interactablePrefabIds.clear();
  g_interactableRest.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (raw.empty())
    return false;
  const std::string cleaned = stripLineCommentsOutsideStrings(raw);
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(cleaned);
  } catch (...) {
    g_interactablePrefabIds.clear();
    return false;
  }
  if (!root.is_object() || !root.contains("list") || !root["list"].is_array())
    return false;
  for (const auto &el : root["list"]) {
    if (!typeFieldIsInteractable(el))
      continue;
    std::string prefab;
    if (el.contains("prefab_name") && el["prefab_name"].is_string())
      prefab = el["prefab_name"].get<std::string>();
    if (prefab.empty())
      continue;
    const std::string pkey = lowerAscii(prefab);
    g_interactablePrefabIds.insert(pkey);
    if (el.contains("rest_emission") && el["rest_emission"].is_object()) {
      const auto &re = el["rest_emission"];
      SubterraInteractableRestDef rd;
      if (re.contains("radius")) {
        const auto &rv = re["radius"];
        if (rv.is_number_float())
          rd.rest_radius = rv.get<float>();
        else if (rv.is_number_integer())
          rd.rest_radius = static_cast<float>(rv.get<int>());
      }
      auto rate = [&re](const char *k) -> float {
        if (!re.contains(k))
          return 0.f;
        const auto &v = re[k];
        if (v.is_number_float())
          return v.get<float>();
        if (v.is_number_integer())
          return static_cast<float>(v.get<int>());
        return 0.f;
      };
      rd.stamina_per_sec = rate("stamina_recovery_rate");
      rd.health_per_sec = rate("health_recovery_rate");
      rd.food_per_sec = rate("food_recovery_rate");
      g_interactableRest[pkey] = rd;
    }
  }
  return true;
}

bool SubterraInteractablePrefabNameIsRegistered(std::string_view prefabName) {
  if (prefabName.empty())
    return false;
  return g_interactablePrefabIds.count(lowerAsciiView(prefabName)) != 0;
}

bool SubterraInteractableTryGetRest(std::string_view interactable_type_normalized,
                                    SubterraInteractableRestDef &out) {
  const std::string key = lowerAsciiView(interactable_type_normalized);
  auto it = g_interactableRest.find(key);
  if (it == g_interactableRest.end())
    return false;
  out = it->second;
  return true;
}

} // namespace subterra
