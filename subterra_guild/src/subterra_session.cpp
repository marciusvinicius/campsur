#include "subterra_session.h"
#include "engine.h"
#include "map_events.h"
#include "subterra_components.h"
#include "terrain_loader.h"
#include <memory>
#include <unordered_set>

namespace subterra {

void sessionLog(SubterraSession &session, const std::string &line) {
  if (session.engine)
    session.engine->GetDebugConsole().AddLogLine(line);
}

void SubterraSession::rebuildTriggers() {
  triggers.clear();
  triggerInsidePrevFrame.clear();
  criogenio::Terrain2D *t = world ? world->GetTerrain() : nullptr;
  if (!t)
    return;
  triggers = BuildMapEventTriggers(*t);
}

void SubterraSession::destroyTransientEntities() {
  if (!world)
    return;
  std::unordered_set<criogenio::ecs::EntityId> kill;
  for (criogenio::ecs::EntityId id : world->GetEntitiesWith<MobTag>())
    kill.insert(id);
  for (criogenio::ecs::EntityId id : world->GetEntitiesWith<WorldPickup>())
    kill.insert(id);
  for (criogenio::ecs::EntityId id : kill) {
    if (id != player)
      world->DeleteEntity(id);
  }
}

bool SubterraSession::loadMap(const std::string &path, std::string &errOut) {
  if (!world) {
    errOut = "no world";
    return false;
  }
  try {
    destroyTransientEntities();
    criogenio::Terrain2D terrain = criogenio::TilemapLoader::LoadFromTMX(path);
    world->SetTerrain(std::make_unique<criogenio::Terrain2D>(std::move(terrain)));
    mapPath = path;
    triggerInsidePrevFrame.clear();
    rebuildTriggers();
    return true;
  } catch (const std::exception &e) {
    errOut = e.what();
    return false;
  }
}

void SubterraSession::placePlayerAtSpawn(const std::string &spawnNameOrEmpty) {
  if (!world || player == criogenio::ecs::NULL_ENTITY)
    return;
  auto *tr = world->GetComponent<criogenio::Transform>(player);
  if (!tr)
    return;
  criogenio::Terrain2D *ter = world->GetTerrain();
  float cx = 0.f, cy = 0.f;
  bool have = false;
  if (!spawnNameOrEmpty.empty())
    have = ter && FindSpawnCenter(ter->tmxMeta, spawnNameOrEmpty, cx, cy);
  if (!have && ter)
    have = FindSpawnCenter(ter->tmxMeta, "player_start_position", cx, cy);
  if (!have && ter && ter->LogicalMapWidthTiles() > 0) {
    cx = ter->LogicalMapWidthTiles() * static_cast<float>(ter->GridStepX()) * 0.5f;
    cy = ter->LogicalMapHeightTiles() * static_cast<float>(ter->GridStepY()) * 0.5f;
    have = true;
  }
  if (!have)
    return;
  tr->x = cx - static_cast<float>(playerW) * 0.5f;
  tr->y = cy - static_cast<float>(playerH) * 0.5f;
}

bool SubterraSession::fireTiledEventByKey(const std::string &key) {
  criogenio::Terrain2D *t = world ? world->GetTerrain() : nullptr;
  if (!t)
    return false;
  for (const auto &trg : triggers) {
    if (trg.storage_key != key && trg.event_id != key)
      continue;
    MapEventPayload p = MakePayloadFromTrigger(trg, true);
    mapEvents.dispatch(p);
    DispatchMapEventDefaults(*this, p);
    return true;
  }
  return false;
}

void SubterraSession::emitManual(const std::string &eventId, const std::string &trigger,
                                  const std::string &eventType) {
  MapEventPayload p;
  p.event_id = eventId;
  p.event_trigger = trigger;
  p.event_type = eventType;
  p.manual = true;
  mapEvents.dispatch(p);
  DispatchMapEventDefaults(*this, p);
}

} // namespace subterra
