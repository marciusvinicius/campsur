#include "map_events.h"
#include "subterra_interactable_prefabs.h"
#include "subterra_mob_prefabs.h"
#include "subterra_session.h"
#include "tmx_metadata.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

namespace subterra {
namespace fs = std::filesystem;

static std::string toLower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string SpawnPointProperty(const std::vector<criogenio::TmxProperty> &props) {
  const char *a = criogenio::TmxGetPropertyString(props, "spawn_point", nullptr);
  if (a && a[0] != '\0')
    return std::string(a);
  const char *b = criogenio::TmxGetPropertyString(props, "spaw_point", nullptr);
  if (b && b[0] != '\0')
    return std::string(b);
  const char *c = criogenio::TmxGetPropertyString(props, "spawn_event_id", nullptr);
  return (c && c[0] != '\0') ? std::string(c) : std::string();
}

namespace {

static std::string trimKey(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(0, 1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

static std::string teleportDestinationForObject(const std::vector<criogenio::TmxProperty> &props) {
  const char *t = criogenio::TmxGetPropertyString(props, "teleport_to", nullptr);
  if (t && t[0] != '\0')
    return std::string(t);
  const char *t2 = criogenio::TmxGetPropertyString(props, "target_level", nullptr);
  return (t2 && t2[0] != '\0') ? std::string(t2) : std::string();
}

static std::string eventIdForObject(const std::vector<criogenio::TmxProperty> &props) {
  const char *e = criogenio::TmxGetPropertyString(props, "event_id", nullptr);
  return (e && e[0] != '\0') ? std::string(e) : std::string();
}

/** Same trigger overlap radius as reference `TILED_MAP_TRIGGER_RADIUS` + margin. */
static constexpr float kTiledMapTriggerRadiusPx = 14.f;
static constexpr float kTeleportOutsideMarginPx = 20.f;

static bool objectMatchesSpawnKey(const criogenio::TiledMapObject &o, const std::string &key) {
  if (o.id != 0) {
    std::string idStr = std::to_string(o.id);
    if (idStr == key)
      return true;
  }
  std::string eid = eventIdForObject(o.properties);
  if (!eid.empty() && eid == key)
    return true;
  if (!o.name.empty() && o.name == key)
    return true;
  return false;
}

static void spawnWorldCenterForObject(const criogenio::TiledMapObject &o,
                                     const std::string &teleportTo, float &cx, float &cy) {
  const float ox = o.x;
  const float oy = o.y;
  const float ow = std::max(0.f, o.width);
  const float oh = std::max(0.f, o.height);
  const bool pointGeom = o.point || (ow <= 0.001f && oh <= 0.001f);

  if (teleportTo.empty()) {
    if (pointGeom) {
      cx = ox;
      cy = oy;
    } else {
      cx = ox + ow * 0.5f;
      cy = oy + oh * 0.5f;
    }
    return;
  }
  const float margin = kTiledMapTriggerRadiusPx + kTeleportOutsideMarginPx;
  if (pointGeom) {
    cx = ox;
    cy = oy - margin;
  } else {
    cx = ox + ow * 0.5f;
    cy = oy - margin;
  }
}

static bool jsonBool(const nlohmann::json &obj, const char *key, bool fallback) {
  if (!obj.is_object() || !key)
    return fallback;
  auto it = obj.find(key);
  if (it == obj.end())
    return fallback;
  if (it->is_boolean())
    return it->get<bool>();
  if (it->is_number_integer())
    return it->get<int>() != 0;
  return fallback;
}

static bool jsonRequirementsMatch(const nlohmann::json &required, const nlohmann::json &eventData) {
  if (!required.is_object())
    return true;
  if (!eventData.is_object())
    return false;
  for (auto it = required.begin(); it != required.end(); ++it) {
    if (it->is_null())
      continue;
    auto jt = eventData.find(it.key());
    if (jt == eventData.end())
      return false;
    if (*jt != *it)
      return false;
  }
  return true;
}

static bool payloadMatchesListenerEvent(const MapEventPayload &p, std::string eventLower) {
  eventLower = toLower(std::move(eventLower));
  if (!p.event_trigger.empty() && toLower(p.event_trigger) == eventLower)
    return true;
  if (!p.event_id.empty() && toLower(p.event_id) == eventLower)
    return true;
  if (!p.event_type.empty() && toLower(p.event_type) == eventLower)
    return true;
  return false;
}

} // namespace

static std::string storageKeyForObject(const criogenio::TiledMapObject &o,
                                       const std::vector<criogenio::TmxProperty> &props) {
  const char *eid = criogenio::TmxGetPropertyString(props, "event_id", nullptr);
  if (eid && eid[0] != '\0')
    return std::string(eid);
  if (o.id != 0)
    return std::to_string(o.id);
  return o.name;
}

static bool shouldRegisterTrigger(const criogenio::TiledMapObject &o,
                                  const std::vector<criogenio::TmxProperty> &props) {
  if (!criogenio::TmxGetPropertyBool(props, "activated", true))
    return false;
  auto propStr = [](const std::vector<criogenio::TmxProperty> &pr, const char *key) {
    const char *v = criogenio::TmxGetPropertyString(pr, key, nullptr);
    return v ? std::string(v) : std::string();
  };
  const std::string ga = propStr(props, "gameplay_actions");
  if (propStr(props, "event_trigger").empty() && propStr(props, "event_id").empty()) {
    if (ga.empty())
      return false;
  }
  if (!ga.empty())
    return true;
  std::string t = o.objectType;
  std::transform(t.begin(), t.end(), t.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (t.find("event") != std::string::npos || t == "teleport")
    return true;
  return criogenio::TmxGetPropertyString(props, "event_trigger", nullptr) != nullptr;
}

std::vector<MapEventTrigger> BuildMapEventTriggers(const criogenio::Terrain2D &terrain) {
  std::vector<MapEventTrigger> out;
  for (const auto &group : terrain.tmxMeta.objectGroups) {
    for (const auto &o : group.objects) {
      if (!shouldRegisterTrigger(o, o.properties))
        continue;
      MapEventTrigger tr;
      tr.storage_key = storageKeyForObject(o, o.properties);
      tr.x = o.x;
      tr.y = o.y;
      tr.w = std::max(0.f, o.width);
      tr.h = std::max(0.f, o.height);
      tr.is_point = o.point || (tr.w < 0.5f && tr.h < 0.5f);
      if (tr.is_point) {
        tr.w = 16.f;
        tr.h = 16.f;
        tr.x -= tr.w * 0.5f;
        tr.y -= tr.h * 0.5f;
      }
      auto propStr = [](const std::vector<criogenio::TmxProperty> &pr, const char *key) {
        const char *v = criogenio::TmxGetPropertyString(pr, key, nullptr);
        return v ? std::string(v) : std::string();
      };
      tr.event_id = propStr(o.properties, "event_id");
      tr.event_trigger = propStr(o.properties, "event_trigger");
      tr.event_type = propStr(o.properties, "event_type");
      tr.object_type = o.objectType;
      {
        std::string tel = propStr(o.properties, "teleport_to");
        if (tel.empty())
          tel = propStr(o.properties, "target_level");
        tr.teleport_to = std::move(tel);
      }
      tr.spawn_point = SpawnPointProperty(o.properties);
      tr.spawn_count = criogenio::TmxGetPropertyInt(o.properties, "spawn_count", 0);
      if (tr.spawn_count <= 0)
        tr.spawn_count = criogenio::TmxGetPropertyInt(o.properties, "monster_count", 0);
      {
        const char *j = criogenio::TmxGetPropertyString(o.properties, "gameplay_actions", nullptr);
        tr.gameplay_actions = (j && j[0] != '\0') ? std::string(j) : std::string();
      }
      out.push_back(std::move(tr));
    }
  }
  return out;
}

bool TriggerStringIsMonsterWave(const std::string &tr) {
  std::string low = toLower(tr);
  bool hasTarget =
      low.find("monster") != std::string::npos || low.find("moster") != std::string::npos ||
      low.find("enemy") != std::string::npos;
  bool hasSpawn = low.find("spawn") != std::string::npos || low.find("spaw") != std::string::npos;
  return hasTarget && hasSpawn;
}

bool TriggerStringIsTeleport(const std::string &tr) {
  std::string low = toLower(tr);
  return low.find("teleport") != std::string::npos || low.find("warp") != std::string::npos;
}

MapEventPayload MakePayloadFromTrigger(const MapEventTrigger &t, bool manual) {
  MapEventPayload p;
  p.event_id = t.event_id;
  p.event_trigger = t.event_trigger;
  p.event_type = t.event_type;
  p.object_type = t.object_type;
  p.teleport_to = t.teleport_to;
  p.spawn_point = t.spawn_point;
  p.world_x = t.x;
  p.world_y = t.y;
  p.world_w = t.w;
  p.world_h = t.h;
  p.is_point = t.is_point;
  p.spawn_count = t.spawn_count;
  p.manual = manual;
  p.gameplay_actions = t.gameplay_actions;
  return p;
}

void MapEventBus::addListener(std::function<void(const MapEventPayload &)> fn) {
  listeners_.push_back(std::move(fn));
}

void MapEventBus::clear() { listeners_.clear(); }

void MapEventBus::dispatch(const MapEventPayload &p) const {
  for (const auto &fn : listeners_)
    fn(p);
}

bool FindSpawnCenter(const criogenio::TmxMapMetadata &meta, const std::string &name,
                     float &outCx, float &outCy) {
  const std::string key = trimKey(name);
  if (key.empty())
    return false;

  auto tryPass = [&](bool requireNoTeleport) -> bool {
    for (const auto &g : meta.objectGroups) {
      for (const auto &o : g.objects) {
        if (!objectMatchesSpawnKey(o, key))
          continue;
        const std::string tel = teleportDestinationForObject(o.properties);
        if (requireNoTeleport && !tel.empty())
          continue;
        spawnWorldCenterForObject(o, tel, outCx, outCy);
        return true;
      }
    }
    return false;
  };

  if (tryPass(true))
    return true;
  return tryPass(false);
}

std::string InteractableStateKey(const std::string &mapPath,
                                const criogenio::TiledInteractable &it) {
  std::string stem = fs::path(mapPath).stem().string();
  char buf[384];
  std::snprintf(buf, sizeof buf, "%s#%s#%d", stem.c_str(), it.interactable_type.c_str(),
                it.tiled_object_id);
  return std::string(buf);
}

std::uint8_t InteractableFlagsEffective(const SubterraSession &session,
                                        const criogenio::TiledInteractable &it) {
  const std::string key = InteractableStateKey(session.mapPath, it);
  auto found = session.interactableStateFlags.find(key);
  if (found != session.interactableStateFlags.end())
    return found->second;
  return 0;
}

nlohmann::json &EnsureInteractableEntityData(SubterraSession &session,
                                             const criogenio::TiledInteractable &it) {
  const std::string key = InteractableStateKey(session.mapPath, it);
  auto found = session.interactableEntityDataByKey.find(key);
  if (found != session.interactableEntityDataByKey.end())
    return found->second;

  nlohmann::json def = nlohmann::json::object();
  SubterraInteractableTryGetDefaultEntityData(toLower(it.interactable_type), def);
  if (!def.is_object())
    def = nlohmann::json::object();

  std::uint8_t flags = InteractableFlagsEffective(session, it);
  if (!def.contains("open") && (flags & InteractableState::Open) != 0)
    def["open"] = true;
  if (!def.contains("burning") && (flags & InteractableState::Burning) != 0)
    def["burning"] = true;

  auto inserted = session.interactableEntityDataByKey.emplace(key, std::move(def));
  return inserted.first->second;
}

const nlohmann::json *FindInteractableEntityData(const SubterraSession &session,
                                                 const criogenio::TiledInteractable &it) {
  const std::string key = InteractableStateKey(session.mapPath, it);
  auto found = session.interactableEntityDataByKey.find(key);
  if (found == session.interactableEntityDataByKey.end())
    return nullptr;
  return &found->second;
}

bool InteractableEntityDataBoolEffective(const SubterraSession &session,
                                         const criogenio::TiledInteractable &it,
                                         const char *key, bool defaultValue) {
  if (!key || key[0] == '\0')
    return defaultValue;
  if (const nlohmann::json *ed = FindInteractableEntityData(session, it))
    return jsonBool(*ed, key, defaultValue);
  const std::uint8_t flags = InteractableFlagsEffective(session, it);
  const std::string low = toLower(key);
  if (low == "open")
    return (flags & InteractableState::Open) != 0;
  if (low == "burning")
    return (flags & InteractableState::Burning) != 0;
  return defaultValue;
}

bool InteractableBlocksMovement(const SubterraSession &session,
                                const criogenio::TiledInteractable &it) {
  if (it.is_point || it.w <= 0.f || it.h <= 0.f)
    return false;
  const std::string kindLower = toLower(it.interactable_type);
  if (kindLower.find("door") == std::string::npos)
    return false;
  return !InteractableEntityDataBoolEffective(session, it, "open", false);
}

bool ClosedDoorOverlapsRect(const SubterraSession &session, float rectLeft, float rectTop,
                            float rectW, float rectH) {
  if (rectW <= 0.f || rectH <= 0.f)
    return false;
  for (const auto &it : session.tiledInteractables) {
    if (!InteractableBlocksMovement(session, it))
      continue;
    const bool overlap = rectLeft < (it.x + it.w) && (rectLeft + rectW) > it.x &&
                         rectTop < (it.y + it.h) && (rectTop + rectH) > it.y;
    if (overlap)
      return true;
  }
  return false;
}

void ApplyInteractableUse(SubterraSession &session, const criogenio::TiledInteractable &it) {
  const std::string kindLower = toLower(it.interactable_type);
  const std::string key = InteractableStateKey(session.mapPath, it);
  std::uint8_t &flags = session.interactableStateFlags[key];
  nlohmann::json &entityData = EnsureInteractableEntityData(session, it);

  auto setOpenState = [&](bool open) {
    entityData["open"] = open;
    if (open)
      flags |= InteractableState::Open;
    else
      flags &= static_cast<std::uint8_t>(~InteractableState::Open);
  };
  auto setBurningState = [&](bool burning) {
    entityData["burning"] = burning;
    if (burning)
      flags |= InteractableState::Burning;
    else
      flags &= static_cast<std::uint8_t>(~InteractableState::Burning);
  };

  if (kindLower == "door") {
    if (jsonBool(entityData, "locked", false)) {
      sessionLog(session, "Door is locked.");
      return;
    }
    const bool nextOpen = !jsonBool(entityData, "open", (flags & InteractableState::Open) != 0);
    setOpenState(nextOpen);
    sessionLog(session, nextOpen ? "Door opened." : "Door closed.");
    return;
  }
  if (kindLower == "chest") {
    if (jsonBool(entityData, "locked", false)) {
      sessionLog(session, "Chest is locked.");
      return;
    }
    const bool nextOpen = !jsonBool(entityData, "open", (flags & InteractableState::Open) != 0);
    setOpenState(nextOpen);
    sessionLog(session, nextOpen ? "Chest opened." : "Chest closed.");
    return;
  }
  if (kindLower == "campfire") {
    const bool nextBurning =
        !jsonBool(entityData, "burning", (InteractableFlagsEffective(session, it) &
                                          InteractableState::Burning) != 0);
    setBurningState(nextBurning);
    sessionLog(session, nextBurning ? "Campfire lit." : "Campfire extinguished.");
    return;
  }
  if (kindLower == "bed") {
    sessionLog(session, "Rest at bed (not wired to vitals yet).");
    return;
  }
  if (kindLower == "lever") {
    const int doorId = criogenio::TmxGetPropertyInt(it.properties, "opens_door", 0);
    if (doorId > 0) {
      const criogenio::TiledInteractable *target = nullptr;
      for (const criogenio::TiledInteractable &cand : session.tiledInteractables) {
        if (cand.tiled_object_id != doorId)
          continue;
        if (toLower(cand.interactable_type) != "door")
          continue;
        target = &cand;
        break;
      }
      if (!target) {
        sessionLog(session, "Lever: no door interactable with that opens_door id.");
        return;
      }
      const std::string dkey = InteractableStateKey(session.mapPath, *target);
      std::uint8_t &dflags = session.interactableStateFlags[dkey];
      nlohmann::json &targetData = EnsureInteractableEntityData(session, *target);
      if (jsonBool(targetData, "locked", false)) {
        sessionLog(session, "Lever pulled: door is locked.");
        return;
      }
      const bool openNow = !jsonBool(targetData, "open", (dflags & InteractableState::Open) != 0);
      targetData["open"] = openNow;
      if (openNow)
        dflags |= InteractableState::Open;
      else
        dflags &= static_cast<std::uint8_t>(~InteractableState::Open);
      sessionLog(session, openNow ? "Lever pulled: door opened." : "Lever pulled: door closed.");
      return;
    }
  }
  if (kindLower == "sign" || kindLower == "lever" || kindLower == "npc") {
    char buf[160];
    std::snprintf(buf, sizeof buf, "Interact: %s", it.interactable_type.c_str());
    sessionLog(session, buf);
    return;
  }
  char buf[192];
  std::snprintf(buf, sizeof buf, "Interact: %s", it.interactable_type.c_str());
  sessionLog(session, buf);
}

void EvaluateInteractableEventListeners(SubterraSession &session, const MapEventPayload &p) {
  for (const criogenio::TiledInteractable &it : session.tiledInteractables) {
    SubterraInteractablePrefabDef def;
    if (!SubterraInteractableTryGetPrefabDef(toLower(it.interactable_type), def))
      continue;
    if (def.event_listeners.empty())
      continue;
    const std::string ikey = InteractableStateKey(session.mapPath, it);
    for (const SubterraInteractableEventListenerDef &listener : def.event_listeners) {
      if (listener.event.empty() || listener.action.empty())
        continue;
      if (!payloadMatchesListenerEvent(p, listener.event))
        continue;
      if (!jsonRequirementsMatch(listener.required_data, p.event_data))
        continue;
      nlohmann::json &entityData = EnsureInteractableEntityData(session, it);
      SubterraGameplayContext ctx{session, &p, nlohmann::json::object()};
      ctx.actionParams["interactable_key"] = ikey;
      ctx.actionParams["interactable_type"] = toLower(it.interactable_type);
      ctx.actionParams["interactable_object_id"] = it.tiled_object_id;
      ctx.actionParams["event"] = listener.event;
      ctx.actionParams["required_data"] = listener.required_data;
      ctx.actionParams["event_data"] = p.event_data;
      ctx.actionParams["entity_data"] = entityData;
      session.gameplay.runAction(listener.action, ctx);
    }
  }
}

void EvaluateMobEventListeners(SubterraSession &session, const MapEventPayload &p) {
  if (!session.world)
    return;
  for (const auto &kv : session.mobPrefabByEntity) {
    const criogenio::ecs::EntityId mobId = kv.first;
    const std::string &prefabId = kv.second;
    if (!session.world->HasEntity(mobId))
      continue;
    SubterraMobPrefabDef def;
    if (!SubterraMobTryGetPrefabDef(prefabId, def))
      continue;
    if (def.event_listeners.empty())
      continue;
    nlohmann::json &mobState = session.mobEntityDataByEntity[mobId];
    if (!mobState.is_object())
      mobState = nlohmann::json::object();
    for (const SubterraMobEventListenerDef &listener : def.event_listeners) {
      if (listener.event.empty() || listener.action.empty())
        continue;
      if (!payloadMatchesListenerEvent(p, listener.event))
        continue;
      if (!jsonRequirementsMatch(listener.required_data, p.event_data))
        continue;
      SubterraGameplayContext ctx{session, &p, nlohmann::json::object()};
      ctx.actionParams["mob_entity_id"] = static_cast<int>(mobId);
      ctx.actionParams["mob_prefab_id"] = prefabId;
      ctx.actionParams["mob_entity_data"] = mobState;
      ctx.actionParams["event_data"] = p.event_data;
      ctx.actionParams["event"] = listener.event;
      session.gameplay.runAction(listener.action, ctx);
    }
  }
}

void DispatchMapEventDefaults(SubterraSession &session, const MapEventPayload &p) {
  session.gameplay.dispatchFromMapEvent(session, p);
}

} // namespace subterra
