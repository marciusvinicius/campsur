#include "subterra_console_commands.h"
#include "subterra_engine.h"

#include "core_systems.h"
#include "components.h"
#include "item_catalog.h"
#include "map_events.h"
#include "spawn_service.h"
#include "subterra_interactable_prefabs.h"
#include "subterra_mob_prefabs.h"
#include "subterra_components.h"
#include "subterra_day_night.h"
#include "subterra_input_config.h"
#include "subterra_player_vitals.h"
#include "subterra_server_config.h"
#include "subterra_session.h"
#include "subterra_gameplay_actions.h"
#include "subterra_status_effects.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace subterra {
namespace {

SubterraSession *g_movementHookSession = nullptr;

bool movementAxisProvider(void *user, criogenio::World &w, criogenio::ecs::EntityId id, float *dx,
                          float *dy) {
  auto *session = static_cast<SubterraSession *>(user);
  return session && SubterraPollPlayerMovementAxes(*session, w, id, dx, dy);
}

bool runHeldProvider(void *user) {
  auto *session = static_cast<SubterraSession *>(user);
  return session && SubterraInputActionDown(*session, "run");
}

bool parseBoolArg(const std::string &a, bool *out) {
  std::string l = a;
  for (char &c : l)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (l == "on" || l == "true" || l == "1") {
    *out = true;
    return true;
  }
  if (l == "off" || l == "false" || l == "0") {
    *out = false;
    return true;
  }
  return false;
}

bool playerWorldCenter(const SubterraSession &session, float *outCx, float *outCy) {
  if (!session.world || session.player == criogenio::ecs::NULL_ENTITY || !outCx || !outCy)
    return false;
  auto *tr = session.world->GetComponent<criogenio::Transform>(session.player);
  if (!tr)
    return false;
  *outCx = tr->x + static_cast<float>(session.playerW) * 0.5f;
  *outCy = tr->y + static_cast<float>(session.playerH) * 0.5f;
  return true;
}

std::string joinArgsFrom(const std::vector<std::string> &args, size_t start) {
  std::string s;
  for (size_t i = start; i < args.size(); ++i) {
    if (i > start)
      s += ' ';
    s += args[i];
  }
  return s;
}

bool isUintString(const std::string &s) {
  if (s.empty())
    return false;
  for (unsigned char uc : s) {
    if (!std::isdigit(uc))
      return false;
  }
  return true;
}

void parseWhTail(const std::vector<std::string> &args, size_t &i, float &w, float &h) {
  while (i < args.size()) {
    if (args[i] == "w" && i + 1 < args.size()) {
      w = std::strtof(args[i + 1].c_str(), nullptr);
      i += 2;
    } else if (args[i] == "h" && i + 1 < args.size()) {
      h = std::strtof(args[i + 1].c_str(), nullptr);
      i += 2;
    } else
      break;
  }
}

} // namespace

void SubterraUnregisterMovementHooks() {
  if (g_movementHookSession && g_movementHookSession->world)
    criogenio::ClearWorldMovementInputProvider(*g_movementHookSession->world);
  g_movementHookSession = nullptr;
}

void RegisterSubterraConsoleCommands(SubterraEngine &engine) {
  criogenio::DebugConsole &c = engine.GetDebugConsole();
  SubterraSession &session = engine.GetSession();
  g_movementHookSession = &session;
  if (session.world) {
    criogenio::SetWorldMovementInputProvider(*session.world, movementAxisProvider, runHeldProvider,
                                             &session);
  }

  c.RegisterCommand("help", [&c](criogenio::Engine &, const std::vector<std::string> &) {
    c.AddLogLine(
        "help clear where debug triggers event emit emitdata emitlight itempairs istate mstate map reload [config] items egui entityinspector intro perfcore netquant netstats");
    c.AddLogLine("spawn mob [n]|<prefab_id> [n] | spawn item|pickup <id> [count] [at x y] [w W] [h H]");
    c.AddLogLine("spawn interact|trigger <text> [at x y] [w W] [h H] [id key] [monsters N] "
                  "[teleport path.tmx]");
    c.AddLogLine("spawn interact clear — remove runtime zones; give/take/drop/inv");
    c.AddLogLine("time | time <0..1> | time noon|dawn|dusk|midnight — daycycle | daycycle <s>|off");
    c.AddLogLine("outdoor [auto|<0..1>] | dayphase <0.05..0.45> — sky/roof (see reference)");
    c.AddLogLine("vitals | vitals set <health|stamina|food> <n> — HUD + vitals (see reference)");
    c.AddLogLine("status apply <flag> | status clear — buffs/debuffers (buffers.json / debuffers.json)");
    c.AddLogLine("respawn — reset vitals + movement after death");
  });

  auto toggleEntityInspector = [&c, &session, &engine](criogenio::Engine &,
                                                     const std::vector<std::string> &) {
    session.showEntityInspector = !session.showEntityInspector;
    if (session.showEntityInspector) {
      engine.GetDebugConsole().Close(engine.GetRenderer().GetWindowHandle());
      c.AddLogLine(
          "Entity Inspector open. Tabs: Mobs, Items, Interactables, Players, States. Close with X.");
    } else {
      c.AddLogLine("Entity Inspector closed.");
    }
  };
  c.RegisterCommand("egui", toggleEntityInspector);
  c.RegisterCommand("entityinspector", toggleEntityInspector);

  c.RegisterCommand("intro", [&c, &session](criogenio::Engine &,
                                            const std::vector<std::string> &) {
    SubterraEnqueueGameplayIntroDemo(session);
    c.AddLogLine("Gameplay intro demo queued (lock → shake → damage → unlock).");
  });

  c.RegisterCommand("perfcore", [&c](criogenio::Engine &, const std::vector<std::string> &args) {
    if (args.size() >= 2 && args[1] == "reset") {
      criogenio::ResetCoreSystemPerfCounters();
      c.AddLogLine("Core perf counters reset.");
      return;
    }
    const auto &perf = criogenio::GetCoreSystemPerfCounters();
    char b[256];
    std::snprintf(b, sizeof b,
                  "Movement(ms): avg=%.3f max=%.3f n=%llu | Animation(ms): avg=%.3f max=%.3f n=%llu",
                  perf.movementAvgMs, perf.movementMaxMs,
                  static_cast<unsigned long long>(perf.movementSamples), perf.animationAvgMs,
                  perf.animationMaxMs, static_cast<unsigned long long>(perf.animationSamples));
    c.AddLogLine(b);
    c.AddLogLine("Usage: perfcore [reset]");
  });

  c.RegisterCommand("where", [&c, &session](criogenio::Engine &,
                                              const std::vector<std::string> &) {
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *tr = session.world->GetComponent<criogenio::Transform>(session.player);
    if (!tr)
      c.AddLogLine("No transform.");
    else {
      char b[160];
      std::snprintf(b, sizeof b, "Player pos: %.1f %.1f (map: %s)", tr->x, tr->y,
                    session.mapPath.c_str());
      c.AddLogLine(b);
    }
  });

  c.RegisterCommand("debug", [&c, &session](criogenio::Engine &,
                                            const std::vector<std::string> &args) {
    if (args.size() <= 1) {
      session.debugOverlay = !session.debugOverlay;
    } else {
      bool v = false;
      if (parseBoolArg(args[1], &v))
        session.debugOverlay = v;
      else {
        c.AddLogLine("Usage: debug [on|off]");
        return;
      }
    }
    c.AddLogLine(session.debugOverlay ? "debug draw: on" : "debug draw: off");
  });

  c.RegisterCommand("triggers", [&c, &session](criogenio::Engine &,
                                                const std::vector<std::string> &args) {
    if (args.size() <= 1) {
      session.debugOverlay = !session.debugOverlay;
    } else {
      bool v = false;
      if (parseBoolArg(args[1], &v))
        session.debugOverlay = v;
    }
    c.AddLogLine(session.debugOverlay ? "trigger overlay: on" : "trigger overlay: off");
  });

  c.RegisterCommand("reload", [&c, &session](criogenio::Engine &,
                                              const std::vector<std::string> &args) {
    bool reloadMap = true;
    if (args.size() >= 2 && args[1] == "config")
      reloadMap = false;
    std::string log;
    SubterraHotReloadServerConfiguration(session, reloadMap, &log);
    for (size_t i = 0; i < log.size();) {
      size_t nl = log.find('\n', i);
      if (nl == std::string::npos) {
        if (i < log.size())
          c.AddLogLine(log.substr(i));
        break;
      }
      if (nl > i)
        c.AddLogLine(log.substr(i, nl - i));
      i = nl + 1;
    }
    if (reloadMap && session.world && session.player != criogenio::ecs::NULL_ENTITY)
      session.placePlayerAtSpawn("");
  });

  c.RegisterCommand("map", [&c, &session](criogenio::Engine &,
                                             const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: map <path.tmx>");
      return;
    }
    std::string path = joinArgsFrom(args, 1);
    std::string err;
    if (session.loadMap(path, err)) {
      session.placePlayerAtSpawn("");
      c.AddLogLine("Loaded " + path);
    } else
      c.AddLogLine(std::string("Failed: ") + err);
  });

  c.RegisterCommand("event", [&c, &session](criogenio::Engine &,
                                             const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: event <event_id|storage_key>");
      return;
    }
    std::string key = joinArgsFrom(args, 1);
    if (session.fireTiledEventByKey(key))
      c.AddLogLine("Dispatched event " + key);
    else
      c.AddLogLine("No trigger with key " + key);
  });

  c.RegisterCommand("emit", [&c, &session](criogenio::Engine &,
                                            const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: emit <trigger_string>");
      return;
    }
    std::string rest = joinArgsFrom(args, 1);
    session.emitManual("", rest, "manual");
    c.AddLogLine("Emitted manual trigger.");
  });

  c.RegisterCommand("emitdata", [&c, &session](criogenio::Engine &,
                                               const std::vector<std::string> &args) {
    if (args.size() < 3) {
      c.AddLogLine("Usage: emitdata <trigger> <json_object>");
      return;
    }
    std::string trigger = args[1];
    std::string jsonText = joinArgsFrom(args, 2);
    nlohmann::json data = nlohmann::json::object();
    try {
      data = nlohmann::json::parse(jsonText);
    } catch (...) {
      c.AddLogLine("emitdata: invalid JSON payload.");
      return;
    }
    if (!data.is_object()) {
      c.AddLogLine("emitdata: payload must be a JSON object.");
      return;
    }
    MapEventPayload p;
    p.event_trigger = trigger;
    p.event_type = "manual";
    p.manual = true;
    p.event_data = std::move(data);
    session.mapEvents.dispatch(p);
    c.AddLogLine("Emitted manual trigger with event_data.");
  });

  c.RegisterCommand("emitlight", [&c, &session](criogenio::Engine &,
                                                const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: emitlight <event> [r g b] [radius] [intensity]");
      return;
    }
    int r = 255, g = 180, b = 80;
    float radius = 128.f;
    float intensity = 1.f;
    if (args.size() >= 5) {
      r = std::clamp(std::atoi(args[2].c_str()), 0, 255);
      g = std::clamp(std::atoi(args[3].c_str()), 0, 255);
      b = std::clamp(std::atoi(args[4].c_str()), 0, 255);
    }
    if (args.size() >= 6)
      radius = std::strtof(args[5].c_str(), nullptr);
    if (args.size() >= 7)
      intensity = std::strtof(args[6].c_str(), nullptr);
    MapEventPayload p;
    p.event_trigger = args[1];
    p.event_type = "manual";
    p.manual = true;
    p.event_data = nlohmann::json::object();
    p.event_data["light_color"] = nlohmann::json::array({r, g, b});
    p.event_data["light_radius"] = radius;
    p.event_data["light_intensity"] = intensity;
    session.mapEvents.dispatch(p);
    c.AddLogLine("Emitted light event payload.");
  });

  c.RegisterCommand("itempairs", [&c, &session](criogenio::Engine &,
                                                const std::vector<std::string> &) {
    c.AddLogLine("Item light overlap pairs: " +
                 std::to_string(static_cast<int>(session.itemEventPairsInside.size())));
    int shown = 0;
    for (const std::string &k : session.itemEventPairsInside) {
      c.AddLogLine(" - " + k);
      shown++;
      if (shown >= 20) {
        c.AddLogLine(" ...");
        break;
      }
    }
  });

  c.RegisterCommand("istate", [&c, &session](criogenio::Engine &,
                                             const std::vector<std::string> &args) {
    const criogenio::TiledInteractable *target = nullptr;
    if (args.size() >= 2) {
      const int objectId = std::atoi(args[1].c_str());
      for (const auto &it : session.tiledInteractables) {
        if (it.tiled_object_id == objectId) {
          target = &it;
          break;
        }
      }
      if (!target) {
        c.AddLogLine("istate: no interactable with that tiled object id.");
        return;
      }
    } else if (session.nearestInteractableIndex >= 0 &&
               session.nearestInteractableIndex < static_cast<int>(session.tiledInteractables.size())) {
      target = &session.tiledInteractables[static_cast<size_t>(session.nearestInteractableIndex)];
    } else {
      c.AddLogLine("istate: no nearby interactable. Usage: istate [tiled_object_id]");
      return;
    }
    const std::string key = InteractableStateKey(session.mapPath, *target);
    nlohmann::json &entityData = EnsureInteractableEntityData(session, *target);
    c.AddLogLine("Interactable key: " + key + " type: " + target->interactable_type);
    c.AddLogLine("entity_data: " + entityData.dump());
  });

  c.RegisterCommand("mstate", [&c, &session](criogenio::Engine &,
                                             const std::vector<std::string> &args) {
    if (!session.world) {
      c.AddLogLine("mstate: no world.");
      return;
    }
    criogenio::ecs::EntityId target = criogenio::ecs::NULL_ENTITY;
    if (args.size() >= 2) {
      target = static_cast<criogenio::ecs::EntityId>(std::atoi(args[1].c_str()));
    } else {
      auto mobs = session.world->GetEntitiesWith<MobTag, criogenio::Transform>();
      if (!mobs.empty())
        target = mobs[0];
    }
    if (target == criogenio::ecs::NULL_ENTITY || !session.world->HasEntity(target)) {
      c.AddLogLine("mstate: no valid mob target. Usage: mstate <entity_id>");
      return;
    }
    auto pit = session.mobPrefabByEntity.find(target);
    auto dit = session.mobEntityDataByEntity.find(target);
    const std::string prefab = pit != session.mobPrefabByEntity.end() ? pit->second : "(unknown)";
    const std::string data =
        dit != session.mobEntityDataByEntity.end() ? dit->second.dump() : std::string("{}");
    c.AddLogLine("mob #" + std::to_string(static_cast<int>(target)) + " prefab=" + prefab);
    c.AddLogLine("mob_state: " + data);
  });

  c.RegisterCommand("items", [&c](criogenio::Engine &, const std::vector<std::string> &) {
    std::vector<std::pair<std::string, std::string>> rows;
    criogenio::ItemCatalog::ListBuiltin(rows);
    for (const auto &pr : rows) {
      c.AddLogLine(pr.first + " — " + pr.second);
    }
    c.AddLogLine("Other ids still work with max stack 99.");
  });

  c.RegisterCommand("time", [&c, &session](criogenio::Engine &,
                                            const std::vector<std::string> &args) {
    SubterraDayNight &dn = session.dayNight;
    if (args.size() <= 1) {
      char buf[220];
      const char *cycle =
          dn.dayDuration > 1e-6 ? "running" : "paused";
      std::snprintf(buf, sizeof buf,
                    "Day %d · t=%.3f (%s) · %s · cycle: %s (%.0fs) · outdoor=%.2f",
                    dn.worldDay, dn.dayTime,
                    SubterraDayPhaseName(dn.dayTime, dn.dayPhaseSize).c_str(),
                    SubterraDayTimeClockLabel(dn.dayTime).c_str(), cycle, dn.dayDuration,
                    static_cast<double>(dn.outdoorFactor));
      c.AddLogLine(buf);
      return;
    }
    std::string a = args[1];
    for (char &ch : a)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (a == "noon")
      dn.dayTime = 0.5;
    else if (a == "midnight")
      dn.dayTime = 0.0;
    else if (a == "dawn")
      dn.dayTime = 0.25;
    else if (a == "dusk")
      dn.dayTime = 0.75;
    else {
      char *end = nullptr;
      double v = std::strtod(args[1].c_str(), &end);
      if (!end || end == args[1].c_str()) {
        c.AddLogLine("Usage: time | time <0..1> | time noon|dawn|dusk|midnight");
        return;
      }
      while (v >= 1.0)
        v -= 1.0;
      while (v < 0.0)
        v += 1.0;
      dn.dayTime = v;
    }
    c.AddLogLine(std::string("Time → ") + SubterraDayTimeClockLabel(dn.dayTime) + " (" +
                 SubterraDayPhaseName(dn.dayTime, dn.dayPhaseSize) + ")");
  });

  c.RegisterCommand("daycycle", [&c, &session](criogenio::Engine &,
                                                const std::vector<std::string> &args) {
    SubterraDayNight &dn = session.dayNight;
    if (args.size() <= 1) {
      if (dn.dayDuration <= 1e-6)
        c.AddLogLine("Day cycle: paused");
      else {
        char b[80];
        std::snprintf(b, sizeof b, "Day cycle: %.0f seconds/day", dn.dayDuration);
        c.AddLogLine(b);
      }
      return;
    }
    std::string a = args[1];
    for (char &ch : a)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (a == "off") {
      dn.dayDuration = 0.0;
      c.AddLogLine("Day cycle paused.");
      return;
    }
    double v = std::strtod(args[1].c_str(), nullptr);
    if (v < 10.0) {
      c.AddLogLine("Usage: daycycle | daycycle <seconds, min 10> | daycycle off");
      return;
    }
    dn.dayDuration = v;
    char b[96];
    std::snprintf(b, sizeof b, "Day cycle set to %.0f seconds/day.", dn.dayDuration);
    c.AddLogLine(b);
  });

  c.RegisterCommand("outdoor", [&c, &session](criogenio::Engine &,
                                               const std::vector<std::string> &args) {
    SubterraDayNight &dn = session.dayNight;
    if (args.size() <= 1) {
      char b[120];
      std::snprintf(b, sizeof b,
                    "outdoor_factor = %.3f (roof_auto=%s) — use 'outdoor auto' or 'outdoor <0..1>'",
                    dn.outdoorFactor, dn.outdoorFromRoof ? "on" : "off");
      c.AddLogLine(b);
      return;
    }
    std::string a1 = args[1];
    for (char &ch : a1)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (a1 == "auto") {
      dn.outdoorFromRoof = true;
      c.AddLogLine("outdoor: roof-driven lerp enabled (reference)");
      return;
    }
    float v = std::strtof(args[1].c_str(), nullptr);
    dn.outdoorFactor = std::clamp(v, 0.f, 1.f);
    dn.outdoorFromRoof = false;
    char b[64];
    std::snprintf(b, sizeof b, "outdoor_factor = %.3f (roof auto off)", dn.outdoorFactor);
    c.AddLogLine(b);
  });

  c.RegisterCommand("dayphase", [&c, &session](criogenio::Engine &,
                                                const std::vector<std::string> &args) {
    SubterraDayNight &dn = session.dayNight;
    if (args.size() <= 1) {
      char b[80];
      std::snprintf(b, sizeof b, "day_time_phase_size = %.3f", dn.dayPhaseSize);
      c.AddLogLine(b);
      return;
    }
    double v = std::strtod(args[1].c_str(), nullptr);
    dn.dayPhaseSize = v;
    SubterraClampDayPhaseSize(dn);
    char b[80];
    std::snprintf(b, sizeof b, "day_time_phase_size = %.3f (clamped)", dn.dayPhaseSize);
    c.AddLogLine(b);
  });

  c.RegisterCommand("spawn", [&c, &session](criogenio::Engine &,
                                              const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: spawn mob [n] | item … | interact … (see help)");
      return;
    }
    std::string kind = args[1];
    for (char &ch : kind)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (kind == "mob") {
      std::string mobPrefabId = "skeleton";
      int n = 1;
      if (args.size() >= 3) {
        if (isUintString(args[2])) {
          n = std::atoi(args[2].c_str());
        } else {
          mobPrefabId = args[2];
          if (!SubterraMobPrefabNameIsRegistered(mobPrefabId))
            c.AddLogLine("spawn mob: unknown mob prefab id, using fallback behavior.");
          if (args.size() >= 4 && isUintString(args[3]))
            n = std::atoi(args[3].c_str());
        }
      }
      if (n < 1)
        n = 1;
      if (n > 50)
        n = 50;
      float cx = 0, cy = 0;
      if (!playerWorldCenter(session, &cx, &cy)) {
        c.AddLogLine("No player position.");
        return;
      }
      SpawnMobsAround(session, cx, cy, n, mobPrefabId);
      c.AddLogLine("Spawned " + std::to_string(n) + " mob(s) of prefab " + mobPrefabId + ".");
      return;
    }
    if (kind == "item" || kind == "pickup") {
      if (args.size() < 3) {
        c.AddLogLine(
            "Usage: spawn item|pickup <item_id> [count] [at x y] [w W] [h H] — default at feet");
        return;
      }
      size_t i = 2;
      std::string itemId = args[i++];
      if (SubterraInteractablePrefabNameIsRegistered(itemId)) {
        c.AddLogLine(itemId +
                     " is an interactable prefab (see entities_interactable.json). "
                     "Place it in Tiled as type/name \"interactable\" with "
                     "interactable_type / prefab_name — not as a world pickup.");
        return;
      }
      int count = 1;
      if (i < args.size() && args[i] != "at" && args[i] != "w" && args[i] != "h" &&
          isUintString(args[i]))
        count = std::atoi(args[i++].c_str());
      if (count < 1)
        count = 1;
      if (count > 999)
        count = 999;
      float cx = 0.f, cy = 0.f;
      float pw = 0.f, ph = 0.f;
      if (i < args.size() && args[i] == "at") {
        if (i + 2 >= args.size()) {
          c.AddLogLine("at needs world x y");
          return;
        }
        cx = std::strtof(args[i + 1].c_str(), nullptr);
        cy = std::strtof(args[i + 2].c_str(), nullptr);
        i += 3;
        parseWhTail(args, i, pw, ph);
      } else {
        if (!playerWorldCenter(session, &cx, &cy)) {
          c.AddLogLine("No player position.");
          return;
        }
        cy += static_cast<float>(session.playerH) * 0.35f;
        parseWhTail(args, i, pw, ph);
      }
      SpawnPickupAt(session, cx, cy, itemId, count, pw, ph);
      c.AddLogLine("Pickup " + itemId + " x" + std::to_string(count) + " @ " +
                   std::to_string(static_cast<int>(cx)) + "," + std::to_string(static_cast<int>(cy)));
      return;
    }
    if (kind == "interact" || kind == "trigger") {
      if (args.size() >= 3 && args[2] == "clear") {
        session.clearRuntimeTriggers();
        c.AddLogLine("Cleared runtime interact triggers.");
        return;
      }
      size_t i = 2;
      std::string trigger;
      while (i < args.size() && args[i] != "at" && args[i] != "w" && args[i] != "h" &&
             args[i] != "monsters" && args[i] != "teleport" && args[i] != "id") {
        if (!trigger.empty())
          trigger += ' ';
        trigger += args[i++];
      }
      if (trigger.empty()) {
        c.AddLogLine("Usage: spawn interact <event_trigger> [at x y] [w W] [h H] [id key] "
                      "[monsters N] [teleport path.tmx]");
        c.AddLogLine("spawn interact clear — remove all runtime zones");
        return;
      }
      float cx = 0.f, cy = 0.f, w = 64.f, h = 48.f;
      std::string event_id = trigger;
      int monsters = 0;
      std::string teleport_to;
      if (i < args.size() && args[i] == "at") {
        if (i + 2 >= args.size()) {
          c.AddLogLine("at needs world x y (center of zone)");
          return;
        }
        cx = std::strtof(args[i + 1].c_str(), nullptr);
        cy = std::strtof(args[i + 2].c_str(), nullptr);
        i += 3;
      } else {
        if (!playerWorldCenter(session, &cx, &cy)) {
          c.AddLogLine("No player position.");
          return;
        }
      }
      parseWhTail(args, i, w, h);
      while (i < args.size()) {
        if (args[i] == "id" && i + 1 < args.size()) {
          event_id = args[i + 1];
          i += 2;
        } else if (args[i] == "monsters" && i + 1 < args.size()) {
          monsters = std::atoi(args[i + 1].c_str());
          i += 2;
        } else if (args[i] == "teleport") {
          teleport_to = joinArgsFrom(args, i + 1);
          i = args.size();
          break;
        } else
          break;
      }
      if (w < 8.f)
        w = 8.f;
      if (h < 8.f)
        h = 8.f;
      MapEventTrigger tr;
      tr.x = cx - w * 0.5f;
      tr.y = cy - h * 0.5f;
      tr.w = w;
      tr.h = h;
      tr.is_point = false;
      tr.event_trigger = trigger;
      tr.event_id = event_id;
      tr.event_type = "interact";
      tr.object_type = "Interactable";
      tr.spawn_count = monsters;
      tr.teleport_to = teleport_to;
      std::string sk = session.addRuntimeTrigger(std::move(tr));
      char buf[200];
      std::snprintf(buf, sizeof buf, "Interact zone key=%s trigger=\"%s\" (toggle debug; test: "
                                     "event %s)",
                    sk.c_str(), trigger.c_str(), sk.c_str());
      c.AddLogLine(buf);
      return;
    }
    c.AddLogLine("Usage: spawn mob [n] | item … | interact … (see help)");
  });

  c.RegisterCommand("respawn", [&c, &session](criogenio::Engine &,
                                              const std::vector<std::string> &) {
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *vit = session.world->GetComponent<PlayerVitals>(session.player);
    auto *ctrl = session.world->GetComponent<criogenio::Controller>(session.player);
    if (!vit) {
      c.AddLogLine("No PlayerVitals component.");
      return;
    }
    vit->active_statuses.clear();
    SubterraInitPlayerVitals(*vit, session.worldRules);
    if (ctrl)
      ctrl->movement_frozen = false;
    c.AddLogLine("Vitals reset (respawn).");
  });

  c.RegisterCommand("status", [&c, &session](criogenio::Engine &,
                                               const std::vector<std::string> &args) {
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *vit = session.world->GetComponent<PlayerVitals>(session.player);
    if (!vit) {
      c.AddLogLine("No PlayerVitals.");
      return;
    }
    if (args.size() < 2) {
      c.AddLogLine("Usage: status apply <player_effect_flag> | status clear");
      return;
    }
    std::string sub = args[1];
    for (char &ch : sub)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (sub == "clear") {
      vit->active_statuses.clear();
      c.AddLogLine("Cleared active statuses.");
      return;
    }
    if (sub == "apply") {
      if (args.size() < 3) {
        c.AddLogLine("Usage: status apply <flag>  (e.g. regen, bleeding, starving)");
        return;
      }
      if (SubterraStatusApply(session, *vit, args[2]))
        c.AddLogLine("Applied status: " + args[2]);
      else
        c.AddLogLine("Unknown status flag (not in buffers/debuffers JSON).");
      return;
    }
    c.AddLogLine("Usage: status apply <flag> | status clear");
  });

  c.RegisterCommand("vitals", [&c, &session](criogenio::Engine &,
                                             const std::vector<std::string> &args) {
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *vit = session.world->GetComponent<PlayerVitals>(session.player);
    if (!vit) {
      c.AddLogLine("No PlayerVitals.");
      return;
    }
    if (args.size() <= 1) {
      char b[200];
      std::snprintf(b, sizeof b, "HP %.0f/%.0f  ST %.0f/%.0f  Food %.0f/%.0f  dead=%d  statuses=%zu",
                    vit->health, vit->health_max, vit->stamina, vit->stamina_max, vit->food_satiety,
                    vit->food_satiety_max, vit->dead ? 1 : 0, vit->active_statuses.size());
      c.AddLogLine(b);
      return;
    }
    if (args.size() < 4 || args[1] != "set") {
      c.AddLogLine("Usage: vitals | vitals set <health|stamina|food> <value>");
      return;
    }
    std::string which = args[2];
    for (char &ch : which)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    float v = std::strtof(args[3].c_str(), nullptr);
    if (which == "health" || which == "hp") {
      vit->health = std::clamp(v, 0.f, vit->health_max);
      if (vit->health > 0.f)
        vit->dead = false;
    } else if (which == "stamina" || which == "stam") {
      vit->stamina = std::clamp(v, 0.f, vit->stamina_max);
    } else if (which == "food" || which == "satiety" || which == "hunger") {
      vit->food_satiety = std::clamp(v, 0.f, vit->food_satiety_max);
    } else {
      c.AddLogLine("Unknown vital (health|stamina|food).");
      return;
    }
    if (auto *ctrl = session.world->GetComponent<criogenio::Controller>(session.player))
      ctrl->movement_frozen = false;
    c.AddLogLine("Vital updated.");
  });

  c.RegisterCommand("give", [&c, &session](criogenio::Engine &,
                                            const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: give <item_id> [count]");
      return;
    }
    std::string itemId = args[1];
    int cnt = 1;
    if (args.size() >= 3)
      cnt = std::atoi(args[2].c_str());
    if (cnt < 1)
      cnt = 1;
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *inv = session.world->GetComponent<Inventory>(session.player);
    if (!inv) {
      c.AddLogLine("No inventory.");
      return;
    }
    int left = inv->TryAdd(itemId, cnt);
    int added = cnt - left;
    if (added <= 0)
      c.AddLogLine("Inventory full.");
    else if (left == 0)
      c.AddLogLine("Added " + criogenio::ItemCatalog::DisplayName(itemId) + " x" +
                   std::to_string(added));
    else {
      c.AddLogLine("Added " + std::to_string(added) + "; " + std::to_string(left) +
                   " did not fit.");
    }
  });

  auto takeOrDrop = [&c, &session](criogenio::Engine &, const std::vector<std::string> &args,
                                   const char *opName, bool dropPickup) {
    if (args.size() < 2) {
      c.AddLogLine(std::string("Usage: ") + opName + " <item_id> [count]");
      return;
    }
    std::string itemId = args[1];
    int cnt = 1;
    if (args.size() >= 3)
      cnt = std::atoi(args[2].c_str());
    if (cnt < 1)
      cnt = 1;
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *inv = session.world->GetComponent<Inventory>(session.player);
    if (!inv) {
      c.AddLogLine("No inventory.");
      return;
    }
    int removed = inv->TryRemove(itemId, cnt);
    if (removed <= 0)
      c.AddLogLine("None removed.");
    else {
      c.AddLogLine("Removed " + std::to_string(removed) + ".");
      if (dropPickup) {
        if (SubterraInteractablePrefabNameIsRegistered(itemId))
          c.AddLogLine("Not spawned as world pickup (interactable prefab id).");
        else {
          float cx = 0, cy = 0;
          if (playerWorldCenter(session, &cx, &cy))
            SpawnPickupAt(session, cx, cy + static_cast<float>(session.playerH) * 0.4f, itemId,
                          removed);
        }
      }
    }
  };

  c.RegisterCommand("take", [takeOrDrop](criogenio::Engine &e,
                                         const std::vector<std::string> &args) {
    takeOrDrop(e, args, "take", false);
  });

  c.RegisterCommand("drop", [takeOrDrop](criogenio::Engine &e,
                                         const std::vector<std::string> &args) {
    takeOrDrop(e, args, "drop", true);
  });

  auto showInv = [&c, &session](criogenio::Engine &, const std::vector<std::string> &) {
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *inv = session.world->GetComponent<Inventory>(session.player);
    if (!inv)
      c.AddLogLine("No inventory.");
    else
      c.AddLogLine(inv->FormatLine());
  };

  c.RegisterCommand("inv", showInv);
  c.RegisterCommand("inventory", showInv);
}

} // namespace subterra
