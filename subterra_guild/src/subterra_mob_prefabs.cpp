#include "subterra_mob_prefabs.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace subterra {
namespace {

std::unordered_map<std::string, SubterraMobPrefabDef> g_mobDefs;

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string lowerAsciiView(std::string_view v) {
  std::string s(v.begin(), v.end());
  return lowerAscii(std::move(s));
}

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

int numberToInt(const nlohmann::json &j, int fallback = 0) {
  if (j.is_number_integer())
    return j.get<int>();
  if (j.is_number_float())
    return static_cast<int>(j.get<float>());
  return fallback;
}

bool isMobType(const nlohmann::json &el) {
  if (!el.is_object() || !el.contains("type") || !el["type"].is_string())
    return false;
  const std::string type = lowerAscii(el["type"].get<std::string>());
  return type == "enemy" || type == "mob" || type == "npc";
}

static bool AppendMobDefsFromList(const nlohmann::json &list,
                                  std::vector<SubterraMobPrefabDef> *outVec,
                                  std::unordered_map<std::string, SubterraMobPrefabDef> *outMap) {
  if (!list.is_array())
    return false;
  for (const auto &el : list) {
    if (!isMobType(el))
      continue;
    SubterraMobPrefabDef def;
    if (el.contains("prefab_name") && el["prefab_name"].is_string())
      def.prefab_name = lowerAscii(el["prefab_name"].get<std::string>());
    if (def.prefab_name.empty() && el.contains("name") && el["name"].is_string()) {
      def.prefab_name = lowerAscii(el["name"].get<std::string>());
      for (char &c : def.prefab_name) {
        if (c == ' ')
          c = '_';
      }
    }
    if (def.prefab_name.empty())
      continue;
    if (el.contains("name") && el["name"].is_string())
      def.display_name = el["name"].get<std::string>();
    if (el.contains("size"))
      def.size = std::max(1, numberToInt(el["size"], 64));
    if (el.contains("texture") && el["texture"].is_string())
      def.texture_path = el["texture"].get<std::string>();
    if (el.contains("position_on_texture") && el["position_on_texture"].is_array() &&
        el["position_on_texture"].size() >= 2) {
      def.texture_x = numberToInt(el["position_on_texture"][0], 0);
      def.texture_y = numberToInt(el["position_on_texture"][1], 0);
    }
    if (el.contains("animation_path") && el["animation_path"].is_string())
      def.animation_path = el["animation_path"].get<std::string>();
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
        SubterraMobEventListenerDef listener;
        listener.event = lowerAscii(evt["event"].get<std::string>());
        listener.action = evt["action"].get<std::string>();
        if (evt.contains("required_data") && evt["required_data"].is_object())
          listener.required_data = evt["required_data"];
        def.event_listeners.push_back(std::move(listener));
      }
    }
    if (outVec)
      outVec->push_back(def);
    if (outMap)
      (*outMap)[def.prefab_name] = std::move(def);
  }
  return true;
}

static bool ParseMobMetaRoot(const nlohmann::json &root,
                             std::vector<SubterraMobPrefabDef> *outVec,
                             std::unordered_map<std::string, SubterraMobPrefabDef> *outMap) {
  if (!root.is_object() || !root.contains("list"))
    return false;
  return AppendMobDefsFromList(root["list"], outVec, outMap);
}

} // namespace

void SubterraMobPrefabsClear() { g_mobDefs.clear(); }

bool SubterraMobPrefabsTryLoadFromPath(const std::string &path) {
  g_mobDefs.clear();
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
    g_mobDefs.clear();
    return false;
  }
  if (!ParseMobMetaRoot(root, nullptr, &g_mobDefs))
    return false;
  return !g_mobDefs.empty();
}

bool SubterraMobPrefabsReadListFromPath(const std::string &path,
                                        std::vector<SubterraMobPrefabDef> &out) {
  out.clear();
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
    return false;
  }
  if (!ParseMobMetaRoot(root, &out, nullptr))
    return false;
  return !out.empty();
}

bool SubterraMobPrefabNameIsRegistered(std::string_view prefabName) {
  if (prefabName.empty())
    return false;
  return g_mobDefs.count(lowerAsciiView(prefabName)) != 0;
}

bool SubterraMobTryGetPrefabDef(std::string_view prefabName, SubterraMobPrefabDef &out) {
  if (prefabName.empty())
    return false;
  auto it = g_mobDefs.find(lowerAsciiView(prefabName));
  if (it == g_mobDefs.end())
    return false;
  out = it->second;
  return true;
}

} // namespace subterra
