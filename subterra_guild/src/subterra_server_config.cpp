#include "subterra_server_config.h"
#include "subterra_camera.h"
#include "subterra_day_night.h"
#include "subterra_input_config.h"
#include "subterra_interactable_prefabs.h"
#include "subterra_item_consumable.h"
#include "subterra_item_light.h"
#include "subterra_json_strip.h"
#include "subterra_mob_prefabs.h"
#include "subterra_player_vitals.h"
#include "subterra_prefab_paths.h"
#include "subterra_session.h"

#include "components.h"
#include "json.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace subterra {
namespace {

float jsonGetF(const nlohmann::json &o, const char *key, float fallback) {
  if (!o.contains(key))
    return fallback;
  const auto &v = o[key];
  if (v.is_number_float())
    return v.get<float>();
  if (v.is_number_integer())
    return static_cast<float>(v.get<int>());
  return fallback;
}

bool jsonGetB(const nlohmann::json &o, const char *key, bool fallback) {
  if (!o.contains(key))
    return fallback;
  const auto &v = o[key];
  if (v.is_boolean())
    return v.get<bool>();
  if (v.is_number_integer())
    return v.get<int>() != 0;
  if (v.is_string()) {
    std::string s = v.get<std::string>();
    for (char &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s == "true" || s == "1";
  }
  return fallback;
}

void parseVec2(const nlohmann::json &o, criogenio::Vec2 &out) {
  if (!o.is_object())
    return;
  out.x = jsonGetF(o, "x", out.x);
  out.y = jsonGetF(o, "y", out.y);
}

void parseShakeEvent(const nlohmann::json &o, SubterraCameraShakeEventCfg &ev) {
  if (!o.is_object())
    return;
  ev.intensity = jsonGetF(o, "intensity", ev.intensity);
  ev.duration = jsonGetF(o, "duration", ev.duration);
  ev.frequency = jsonGetF(o, "frequency", ev.frequency);
  ev.decay = jsonGetF(o, "decay", ev.decay);
  if (o.contains("direction") && o["direction"].is_object()) {
    const auto &d = o["direction"];
    ev.dir_x = jsonGetF(d, "x", ev.dir_x);
    ev.dir_y = jsonGetF(d, "y", ev.dir_y);
  }
}

void parseZoomEvent(const nlohmann::json &o, SubterraCameraZoomEventCfg &ev) {
  if (!o.is_object())
    return;
  ev.multiplier = jsonGetF(o, "multiplier", ev.multiplier);
  ev.speed = jsonGetF(o, "speed", ev.speed);
}

void parseCameraFromRoot(const nlohmann::json &root, SubterraCameraBundle &cam) {
  if (!root.contains("camera") || !root["camera"].is_object())
    return;
  const nlohmann::json &c = root["camera"];
  cam.cfg.zoom = jsonGetF(c, "zoom", cam.cfg.zoom);
  cam.cfg.follow_player = jsonGetB(c, "follow_player", cam.cfg.follow_player);
  if (c.contains("follow_player_offset") && c["follow_player_offset"].is_object())
    parseVec2(c["follow_player_offset"], cam.cfg.follow_offset);
  cam.cfg.follow_deadzone_half_w =
      jsonGetF(c, "follow_deadzone_half_w", cam.cfg.follow_deadzone_half_w);
  cam.cfg.follow_deadzone_half_h =
      jsonGetF(c, "follow_deadzone_half_h", cam.cfg.follow_deadzone_half_h);
  cam.cfg.follow_smoothing_speed =
      jsonGetF(c, "follow_smoothing_speed", cam.cfg.follow_smoothing_speed);
  cam.cfg.zoom_active = jsonGetB(c, "zoom_active", cam.cfg.zoom_active);
  cam.cfg.shake_active = jsonGetB(c, "shake_active", cam.cfg.shake_active);
  if (c.contains("zoom_on") && c["zoom_on"].is_object()) {
    const auto &z = c["zoom_on"];
    if (z.contains("on_run_start"))
      parseZoomEvent(z["on_run_start"], cam.cfg.zoom_on_run_start);
    if (z.contains("on_run_stop"))
      parseZoomEvent(z["on_run_stop"], cam.cfg.zoom_on_run_stop);
    if (z.contains("on_run"))
      parseZoomEvent(z["on_run"], cam.cfg.zoom_on_run_alias);
  }
  if (c.contains("shake_on") && c["shake_on"].is_object()) {
    const auto &s = c["shake_on"];
    if (s.contains("on_attack"))
      parseShakeEvent(s["on_attack"], cam.cfg.shake_on_attack);
    if (s.contains("player_attack"))
      parseShakeEvent(s["player_attack"], cam.cfg.shake_player_attack);
    if (s.contains("player_hit"))
      parseShakeEvent(s["player_hit"], cam.cfg.shake_player_hit);
  }
}

bool applyWorldRulesFromRoot(const nlohmann::json &root, SubterraWorldRules &out) {
  if (!root.contains("world") || !root["world"].is_object())
    return false;
  const auto &w = root["world"];
  auto getF = [&w](const char *key, float fallback) -> float {
    return jsonGetF(w, key, fallback);
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

} // namespace

bool SubterraTryLoadServerConfigurationFromPaths(const char *const *paths, size_t pathCount,
                                                 SubterraSession &session) {
  for (size_t i = 0; i < pathCount; ++i) {
    const char *p = paths[i];
    if (!p)
      continue;
    std::ifstream f(p, std::ios::binary);
    if (!f)
      continue;
    std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (raw.empty())
      continue;
    nlohmann::json root;
    try {
      root = nlohmann::json::parse(SubterraStripJsonLineComments(raw));
    } catch (...) {
      continue;
    }
    if (!root.is_object())
      continue;
    if (!applyWorldRulesFromRoot(root, session.worldRules))
      continue;
    parseCameraFromRoot(root, session.camera);
    SubterraCameraResetRuntimeFromConfig(session.camera);
    if (root.contains("world") && root["world"].is_object()) {
      const auto &w = root["world"];
      session.dayNight.dayPhaseSize =
          static_cast<double>(jsonGetF(w, "day_time_phase_size",
                                        static_cast<float>(session.dayNight.dayPhaseSize)));
      SubterraClampDayPhaseSize(session.dayNight);
    }
    if (root.contains("debug") && root["debug"].is_object())
      session.debugModeFromConfig = jsonGetB(root["debug"], "debug_mode", false);
    session.worldConfigPath = p;
    session.configInitMap.clear();
    if (root.contains("world") && root["world"].is_object()) {
      const auto &wobj = root["world"];
      if (wobj.contains("init_map") && wobj["init_map"].is_string())
        session.configInitMap = wobj["init_map"].get<std::string>();
    }
    return true;
  }
  return false;
}

bool SubterraTryLoadInputConfigFromPaths(const char *const *paths, size_t pathCount,
                                         SubterraSession &session) {
  for (size_t i = 0; i < pathCount; ++i) {
    const char *p = paths[i];
    if (!p)
      continue;
    if (SubterraTryLoadInputConfigFromPath(p, session.input)) {
      session.inputConfigPath = p;
      session.inputHotReloadAccum = 0.f;
      return true;
    }
  }
  SubterraInputApplyDefaults(session.input);
  session.inputConfigPath.clear();
  return false;
}

bool SubterraHotReloadServerConfiguration(SubterraSession &session, bool reloadMap,
                                         std::string *logOut) {
  auto log = [&](const char *line) {
    if (logOut) {
      if (!logOut->empty())
        *logOut += '\n';
      *logOut += line;
    }
  };
  bool ok = true;
  if (!session.worldConfigPath.empty()) {
    std::ifstream f(session.worldConfigPath, std::ios::binary);
    if (!f) {
      ok = false;
      log("[Reload] world_config.json missing or unreadable.");
    } else {
      std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      nlohmann::json root;
      try {
        root = nlohmann::json::parse(SubterraStripJsonLineComments(raw));
      } catch (...) {
        ok = false;
        log("[Reload] world_config.json parse error.");
      }
      if (root.is_object() && applyWorldRulesFromRoot(root, session.worldRules)) {
        parseCameraFromRoot(root, session.camera);
        SubterraCameraResetRuntimeFromConfig(session.camera);
        if (root.contains("world") && root["world"].is_object()) {
          const auto &w = root["world"];
          session.dayNight.dayPhaseSize =
              static_cast<double>(jsonGetF(w, "day_time_phase_size",
                                            static_cast<float>(session.dayNight.dayPhaseSize)));
          SubterraClampDayPhaseSize(session.dayNight);
          session.configInitMap.clear();
          if (w.contains("init_map") && w["init_map"].is_string())
            session.configInitMap = w["init_map"].get<std::string>();
        }
        if (root.contains("debug") && root["debug"].is_object())
          session.debugModeFromConfig =
              jsonGetB(root["debug"], "debug_mode", session.debugModeFromConfig);
        log("[Reload] world_config.json applied.");
      }
    }
  } else
    log("[Reload] no world_config path (never loaded successfully).");

  if (!session.inputConfigPath.empty()) {
    if (SubterraTryLoadInputConfigFromPath(session.inputConfigPath, session.input))
      log("[Reload] input_config.json applied.");
    else {
      ok = false;
      log("[Reload] input_config.json failed.");
    }
  } else {
    SubterraInputApplyDefaults(session.input);
    log("[Reload] input_config path empty — defaults.");
  }

  {
    bool itemOk = false;
    for (const char *p : kItemPrefabJsonPaths) {
      if (SubterraItemLight::TryLoadFromPath(p)) {
        log("[Reload] entities_items (lights + event_dispatch): applied.");
        itemOk = true;
        break;
      }
    }
    if (!itemOk)
      log("[Reload] entities_items: could not reload (tried data/... and subterra_guild/...).");

    bool interactOk = false;
    for (const char *p : kInteractablePrefabJsonPaths) {
      if (SubterraInteractablePrefabsTryLoadFromPath(p)) {
        log("[Reload] entities_interactable: applied.");
        interactOk = true;
        break;
      }
    }
    if (!interactOk)
      log("[Reload] entities_interactable: could not reload.");

    bool mobOk = false;
    for (const char *p : kMobPrefabJsonPaths) {
      if (SubterraMobPrefabsTryLoadFromPath(p)) {
        log("[Reload] entities_mobs: applied.");
        mobOk = true;
        break;
      }
    }
    if (!mobOk)
      log("[Reload] entities_mobs: could not reload.");

    bool consumeOk = false;
    for (const char *p : kItemPrefabJsonPaths) {
      if (SubterraItemConsumableTryLoadFromPath(p)) {
        log("[Reload] consumable restore fields (entities_items): applied.");
        consumeOk = true;
        break;
      }
    }
    if (!consumeOk)
      log("[Reload] consumable restore fields: could not reload.");
  }

  if (session.world && session.player != criogenio::ecs::NULL_ENTITY) {
    if (auto *vit = session.world->GetComponent<PlayerVitals>(session.player))
      SubterraInitPlayerVitals(*vit, session.worldRules);
  }

  if (reloadMap && session.world && !session.mapPath.empty()) {
    std::string err;
    if (session.loadMap(session.mapPath, err))
      log("[Reload] map reloaded.");
    else {
      ok = false;
      if (logOut) {
        *logOut += "\n[Reload] map reload failed: ";
        *logOut += err;
      }
    }
  }

  return ok;
}

bool SubterraSaveServerConfiguration(SubterraSession &session, std::string *logOut) {
  auto log = [&](const char *line) {
    if (!logOut)
      return;
    if (!logOut->empty())
      *logOut += '\n';
    *logOut += line;
  };

  if (session.worldConfigPath.empty()) {
    log("[Save] no world_config path (never loaded successfully).");
    return false;
  }

  std::ifstream in(session.worldConfigPath, std::ios::binary);
  if (!in) {
    log("[Save] world_config.json missing or unreadable.");
    return false;
  }

  nlohmann::json root;
  try {
    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    root = nlohmann::json::parse(SubterraStripJsonLineComments(raw));
  } catch (...) {
    log("[Save] world_config.json parse error.");
    return false;
  }
  if (!root.is_object())
    root = nlohmann::json::object();

  if (!root.contains("world") || !root["world"].is_object())
    root["world"] = nlohmann::json::object();
  auto &w = root["world"];
  w["day_time_phase_size"] = session.dayNight.dayPhaseSize;
  w["food_depletion_rate"] = session.worldRules.food_consumption_rate;
  w["health_satiety_depletion_rate"] = session.worldRules.health_consumption_rate;
  w["stamina_depletion_on_run_rate"] = session.worldRules.stamina_consumption_rate;
  w["food_satiety_max"] = session.worldRules.food_max;
  w["health_satiety_max"] = session.worldRules.health_max;
  w["stamina_satiety_max"] = session.worldRules.stamina_max;
  w["food_satiety_regen_rate"] = session.worldRules.food_regen_rate;
  w["health_satiety_regen_rate"] = session.worldRules.health_regen_rate;
  w["stamina_satiety_regen_rate"] = session.worldRules.stamina_regen_rate;

  const auto &cfg = session.camera.cfg;
  if (!root.contains("camera") || !root["camera"].is_object())
    root["camera"] = nlohmann::json::object();
  auto &c = root["camera"];
  c["zoom"] = cfg.zoom;
  c["follow_player"] = cfg.follow_player;
  c["follow_player_offset"] = {{"x", cfg.follow_offset.x}, {"y", cfg.follow_offset.y}};
  c["follow_deadzone_half_w"] = cfg.follow_deadzone_half_w;
  c["follow_deadzone_half_h"] = cfg.follow_deadzone_half_h;
  c["follow_smoothing_speed"] = cfg.follow_smoothing_speed;
  c["zoom_active"] = cfg.zoom_active;
  c["shake_active"] = cfg.shake_active;
  c["zoom_on"] = {
      {"on_run_start",
       {{"multiplier", cfg.zoom_on_run_start.multiplier}, {"speed", cfg.zoom_on_run_start.speed}}},
      {"on_run_stop",
       {{"multiplier", cfg.zoom_on_run_stop.multiplier}, {"speed", cfg.zoom_on_run_stop.speed}}},
      {"on_run",
       {{"multiplier", cfg.zoom_on_run_alias.multiplier}, {"speed", cfg.zoom_on_run_alias.speed}}},
  };
  c["shake_on"] = {
      {"on_attack",
       {{"intensity", cfg.shake_on_attack.intensity},
        {"duration", cfg.shake_on_attack.duration},
        {"frequency", cfg.shake_on_attack.frequency},
        {"decay", cfg.shake_on_attack.decay},
        {"direction",
         {{"x", cfg.shake_on_attack.dir_x}, {"y", cfg.shake_on_attack.dir_y}}}}},
      {"player_attack",
       {{"intensity", cfg.shake_player_attack.intensity},
        {"duration", cfg.shake_player_attack.duration},
        {"frequency", cfg.shake_player_attack.frequency},
        {"decay", cfg.shake_player_attack.decay},
        {"direction",
         {{"x", cfg.shake_player_attack.dir_x}, {"y", cfg.shake_player_attack.dir_y}}}}},
      {"player_hit",
       {{"intensity", cfg.shake_player_hit.intensity},
        {"duration", cfg.shake_player_hit.duration},
        {"frequency", cfg.shake_player_hit.frequency},
        {"decay", cfg.shake_player_hit.decay},
        {"direction",
         {{"x", cfg.shake_player_hit.dir_x}, {"y", cfg.shake_player_hit.dir_y}}}}},
  };

  if (!root.contains("debug") || !root["debug"].is_object())
    root["debug"] = nlohmann::json::object();
  root["debug"]["debug_mode"] = session.debugModeFromConfig;

  std::ofstream out(session.worldConfigPath, std::ios::binary | std::ios::trunc);
  if (!out) {
    log("[Save] failed to open world_config for writing.");
    return false;
  }
  out << root.dump(2);
  if (!out.good()) {
    log("[Save] write failed.");
    return false;
  }

  log("[Save] world_config.json written.");
  return true;
}

} // namespace subterra
