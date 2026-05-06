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
std::unordered_map<std::string, SubterraInteractablePrefabDef> g_interactableDefs;

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

bool typeFieldIsInteractableEventListener(const nlohmann::json &el) {
  if (!el.is_object() || !el.contains("type"))
    return false;
  const auto &t = el["type"];
  if (!t.is_string())
    return false;
  std::string v = lowerAscii(t.get<std::string>());
  return v == "interactable_event_listener";
}

} // namespace

void SubterraInteractablePrefabsClear() {
  g_interactablePrefabIds.clear();
  g_interactableRest.clear();
  g_interactableDefs.clear();
}

bool SubterraInteractablePrefabsTryLoadFromPath(const std::string &path) {
  g_interactablePrefabIds.clear();
  g_interactableRest.clear();
  g_interactableDefs.clear();
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
    const bool is_interactable = typeFieldIsInteractable(el);
    const bool is_listener_only = typeFieldIsInteractableEventListener(el);
    if (!is_interactable && !is_listener_only)
      continue;
    std::string prefab;
    if (el.contains("prefab_name") && el["prefab_name"].is_string())
      prefab = el["prefab_name"].get<std::string>();
    if (prefab.empty())
      continue;
    const std::string pkey = lowerAscii(prefab);
    g_interactablePrefabIds.insert(pkey);
    SubterraInteractablePrefabDef &def = g_interactableDefs[pkey];
    def.is_event_listener_only = is_listener_only;
    def.default_entity_data = nlohmann::json::object();
    def.event_listeners.clear();
    if (el.contains("entity_data") && el["entity_data"].is_object())
      def.default_entity_data = el["entity_data"];
    if (el.contains("event_listeners") && el["event_listeners"].is_array()) {
      for (const auto &evt : el["event_listeners"]) {
        if (!evt.is_object())
          continue;
        if (!evt.contains("event") || !evt["event"].is_string())
          continue;
        if (!evt.contains("action") || !evt["action"].is_string())
          continue;
        SubterraInteractableEventListenerDef listener;
        listener.event = lowerAscii(evt["event"].get<std::string>());
        listener.action = evt["action"].get<std::string>();
        if (evt.contains("required_data") && evt["required_data"].is_object())
          listener.required_data = evt["required_data"];
        def.event_listeners.push_back(std::move(listener));
      }
    }
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

bool SubterraInteractableTryGetPrefabDef(std::string_view interactable_type_normalized,
                                         SubterraInteractablePrefabDef &out) {
  const std::string key = lowerAsciiView(interactable_type_normalized);
  auto it = g_interactableDefs.find(key);
  if (it == g_interactableDefs.end())
    return false;
  out = it->second;
  return true;
}

bool SubterraInteractableTryGetDefaultEntityData(std::string_view interactable_type_normalized,
                                                 nlohmann::json &out) {
  SubterraInteractablePrefabDef def;
  if (!SubterraInteractableTryGetPrefabDef(interactable_type_normalized, def))
    return false;
  out = def.default_entity_data;
  return out.is_object();
}

bool SubterraInteractableTypeCanDirectUse(std::string_view interactable_type_normalized) {
  SubterraInteractablePrefabDef def;
  if (!SubterraInteractableTryGetPrefabDef(interactable_type_normalized, def))
    return true;
  return !def.is_event_listener_only;
}

} // namespace subterra
