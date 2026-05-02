#include "map_events.h"
#include "spawn_service.h"
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
  if (a)
    return std::string(a);
  const char *b = criogenio::TmxGetPropertyString(props, "spaw_point", nullptr);
  return b ? std::string(b) : std::string();
}

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
      tr.teleport_to = propStr(o.properties, "teleport_to");
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
  if (name.empty())
    return false;
  for (const auto &g : meta.objectGroups) {
    for (const auto &o : g.objects) {
      if (o.name == name) {
        if (o.point || (o.width <= 0.001f && o.height <= 0.001f)) {
          outCx = o.x;
          outCy = o.y;
        } else {
          outCx = o.x + o.width * 0.5f;
          outCy = o.y + o.height * 0.5f;
        }
        return true;
      }
    }
  }
  for (const auto &g : meta.objectGroups) {
    for (const auto &o : g.objects) {
      const char *eid = criogenio::TmxGetPropertyString(o.properties, "event_id", nullptr);
      if (eid && name == eid) {
        if (o.point || (o.width <= 0.001f && o.height <= 0.001f)) {
          outCx = o.x;
          outCy = o.y;
        } else {
          outCx = o.x + o.width * 0.5f;
          outCy = o.y + o.height * 0.5f;
        }
        return true;
      }
    }
  }
  return false;
}

void DispatchMapEventDefaults(SubterraSession &session, const MapEventPayload &p) {
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
