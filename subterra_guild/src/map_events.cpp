#include "map_events.h"
#include "spawn_service.h"
#include "subterra_camera.h"
#include "subterra_session.h"
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
  if (propStr(props, "event_trigger").empty() && propStr(props, "event_id").empty())
    return false;
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

void ApplyInteractableUse(SubterraSession &session, const criogenio::TiledInteractable &it) {
  const std::string kindLower = toLower(it.interactable_type);
  const std::string key = InteractableStateKey(session.mapPath, it);
  std::uint8_t &flags = session.interactableStateFlags[key];

  if (kindLower == "door") {
    flags ^= InteractableState::Open;
    sessionLog(session, (flags & InteractableState::Open) ? "Door opened." : "Door closed.");
    return;
  }
  if (kindLower == "chest") {
    flags ^= InteractableState::Open;
    sessionLog(session, (flags & InteractableState::Open) ? "Chest opened." : "Chest closed.");
    return;
  }
  if (kindLower == "campfire") {
    const std::uint8_t cur = InteractableFlagsEffective(session, it);
    flags = cur ^ InteractableState::Burning;
    sessionLog(session,
                (flags & InteractableState::Burning) ? "Campfire lit." : "Campfire extinguished.");
    return;
  }
  if (kindLower == "bed") {
    sessionLog(session, "Rest at bed (not wired to vitals yet).");
    return;
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

void DispatchMapEventDefaults(SubterraSession &session, const MapEventPayload &p) {
  if (!p.event_trigger.empty())
    SubterraCameraNotifyTrigger(session, p.event_trigger);
  if (TriggerStringIsMonsterWave(p.event_trigger)) {
    float cx = p.world_x + p.world_w * 0.5f;
    float cy = p.world_y + p.world_h * 0.5f;
    if (p.is_point)
      cx = p.world_x, cy = p.world_y;
    int n = p.spawn_count > 0 ? p.spawn_count : 5;
    SpawnMobsAround(session, cx, cy, n);
    {
      char buf[96];
      std::snprintf(buf, sizeof buf, "Monster wave: spawned %d at (%.0f,%.0f)", n, cx, cy);
      sessionLog(session, buf);
    }
    return;
  }
  if (!p.teleport_to.empty() && !TriggerStringIsMonsterWave(p.event_trigger)) {
    fs::path base = fs::path(session.mapPath).parent_path();
    fs::path dest = (base / p.teleport_to).lexically_normal();
    std::string err;
    if (session.loadMap(dest.string(), err)) {
      session.placePlayerAtSpawn(p.spawn_point);
      sessionLog(session, "Teleported to " + dest.string());
    } else {
      std::fprintf(stderr, "[MapEvent] teleport failed: %s\n", err.c_str());
      sessionLog(session, std::string("Teleport failed: ") + err);
    }
    return;
  }
  if (!p.event_trigger.empty() || !p.event_id.empty()) {
    std::fprintf(stderr, "[MapEvent] id=%s trigger=%s type=%s manual=%d\n", p.event_id.c_str(),
                 p.event_trigger.c_str(), p.event_type.c_str(), p.manual ? 1 : 0);
  }
}

} // namespace subterra
