#include "subterra_level_ecs.h"
#include "components.h"
#include "map_authoring_components.h"
#include "world.h"
#include "json.hpp"
#include <algorithm>
#include <cctype>

namespace subterra {
namespace {

static std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

static bool ecsZoneRegisters(const criogenio::MapEventZone2D &z) {
  if (!z.activated)
    return false;
  auto empty = [](const std::string &s) { return s.empty(); };
  if (empty(z.event_trigger) && empty(z.event_id)) {
    if (empty(z.gameplay_actions))
      return false;
  }
  if (!empty(z.gameplay_actions))
    return true;
  std::string t = lowerAscii(z.object_type);
  if (t.find("event") != std::string::npos || t == "teleport")
    return true;
  return !z.event_trigger.empty();
}

static void jsonPropsToTmx(const std::string &jsonStr, std::vector<criogenio::TmxProperty> &out) {
  out.clear();
  if (jsonStr.empty())
    return;
  try {
    nlohmann::json j = nlohmann::json::parse(jsonStr);
    if (!j.is_object())
      return;
    for (auto it = j.begin(); it != j.end(); ++it) {
      criogenio::TmxProperty p;
      p.name = it.key();
      if (it->is_string())
        p.value = it->get<std::string>();
      else
        p.value = it->dump();
      p.type = "string";
      out.push_back(std::move(p));
    }
  } catch (...) {
  }
}

} // namespace

void CollectMapEventTriggersFromEntities(criogenio::World &world,
                                        std::vector<MapEventTrigger> &out) {
  for (criogenio::ecs::EntityId e : world.GetEntitiesWith<criogenio::MapEventZone2D,
                                                          criogenio::Transform>()) {
    auto *z = world.GetComponent<criogenio::MapEventZone2D>(e);
    auto *tr = world.GetComponent<criogenio::Transform>(e);
    if (!z || !tr || !ecsZoneRegisters(*z))
      continue;
    MapEventTrigger t;
    t.storage_key = !z->storage_key.empty() ? z->storage_key : ("e" + std::to_string(static_cast<int>(e)));
    t.x = tr->x;
    t.y = tr->y;
    t.w = std::max(0.f, z->width);
    t.h = std::max(0.f, z->height);
    t.is_point = z->point || (t.w < 0.5f && t.h < 0.5f);
    if (t.is_point) {
      t.w = 16.f;
      t.h = 16.f;
      t.x -= t.w * 0.5f;
      t.y -= t.h * 0.5f;
    }
    t.event_id = z->event_id;
    t.event_trigger = z->event_trigger;
    t.event_type = z->event_type;
    t.object_type = z->object_type;
    t.teleport_to = z->teleport_to;
    if (t.teleport_to.empty())
      t.teleport_to = z->target_level;
    t.spawn_point = z->spawn_point;
    t.spawn_count = z->spawn_count;
    if (t.spawn_count <= 0)
      t.spawn_count = z->monster_count;
    t.gameplay_actions = z->gameplay_actions;
    out.push_back(std::move(t));
  }
}

void AppendMapEventTriggersFromEntities(criogenio::World &world,
                                       std::vector<MapEventTrigger> &out) {
  CollectMapEventTriggersFromEntities(world, out);
}

void CollectInteractablesFromEntities(criogenio::World &world,
                                     std::vector<criogenio::TiledInteractable> &out) {
  for (criogenio::ecs::EntityId e : world.GetEntitiesWith<criogenio::InteractableZone2D,
                                                          criogenio::Transform>()) {
    auto *z = world.GetComponent<criogenio::InteractableZone2D>(e);
    auto *tr = world.GetComponent<criogenio::Transform>(e);
    if (!z || !tr || z->interactable_type.empty())
      continue;
    criogenio::TiledInteractable ti;
    ti.x = tr->x;
    ti.y = tr->y;
    ti.w = std::max(0.f, z->width);
    ti.h = std::max(0.f, z->height);
    ti.is_point = z->point || (ti.w < 0.5f && ti.h < 0.5f);
    ti.tiled_object_id =
        z->tiled_object_id != 0 ? z->tiled_object_id : static_cast<int>(e);
    ti.interactable_type = z->interactable_type;
    jsonPropsToTmx(z->properties_json, ti.properties);
    out.push_back(std::move(ti));
  }
}

void AppendInteractablesFromEntities(criogenio::World &world,
                                    std::vector<criogenio::TiledInteractable> &out) {
  CollectInteractablesFromEntities(world, out);
}

} // namespace subterra
