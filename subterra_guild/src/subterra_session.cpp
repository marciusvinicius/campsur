#include "subterra_session.h"
#include "components.h"
#include "engine.h"
#include "input.h"
#include "map_events.h"
#include "spawn_service.h"
#include "subterra_components.h"
#include "terrain_loader.h"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>

namespace subterra {

namespace fs = std::filesystem;

namespace {

static std::string mapBasenameFromPath(const std::string &path) {
  if (path.empty())
    return {};
  return fs::path(path).filename().string();
}

static void snapshotPickupsForBasename(SubterraSession &session, const std::string &basename) {
  if (basename.empty() || !session.world)
    return;
  std::vector<PersistedPickup> list;
  auto ids = session.world->GetEntitiesWith<WorldPickup, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : ids) {
    auto *pk = session.world->GetComponent<WorldPickup>(id);
    auto *tr = session.world->GetComponent<criogenio::Transform>(id);
    if (!pk || !tr || pk->item_id.empty() || pk->count <= 0)
      continue;
    PersistedPickup p;
    p.item_id = pk->item_id;
    p.count = pk->count;
    p.width = pk->width;
    p.height = pk->height;
    p.center_x = tr->x + pk->width * 0.5f;
    p.center_y = tr->y + pk->height * 0.5f;
    list.push_back(std::move(p));
  }
  session.persistedPickupsByBasename[basename] = std::move(list);
}

} // namespace

void sessionLog(SubterraSession &session, const std::string &line) {
  if (session.engine)
    session.engine->GetDebugConsole().AddLogLine(line);
}

void SubterraSession::rebuildTriggers() {
  triggers.clear();
  triggerInsidePrevFrame.clear();
  tiledInteractables.clear();
  criogenio::Terrain2D *t = world ? world->GetTerrain() : nullptr;
  if (t) {
    triggers = BuildMapEventTriggers(*t);
    tiledInteractables = t->tmxMeta.interactables;
  }
  triggers.insert(triggers.end(), runtimeTriggers.begin(), runtimeTriggers.end());
}

std::string SubterraSession::addRuntimeTrigger(MapEventTrigger t) {
  if (t.storage_key.empty())
    t.storage_key = "rt_" + std::to_string(runtimeTriggerSeq++);
  std::string key = t.storage_key;
  runtimeTriggers.push_back(std::move(t));
  rebuildTriggers();
  return key;
}

void SubterraSession::clearRuntimeTriggers() {
  runtimeTriggers.clear();
  runtimeTriggerSeq = 1;
  rebuildTriggers();
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
    if (id != player) {
      world->DeleteEntity(id);
      mobEntityDataByEntity.erase(id);
      mobPrefabByEntity.erase(id);
    }
  }
}

bool SubterraSession::loadMap(const std::string &path, std::string &errOut) {
  if (!world) {
    errOut = "no world";
    return false;
  }
  try {
    const std::string leavingBasename = mapBasenameFromPath(mapPath);
    if (!leavingBasename.empty())
      snapshotPickupsForBasename(*this, leavingBasename);

    destroyTransientEntities();
    gameplayCommandQueue.clear();
    gameplayInputLockDepth = 0;
    criogenio::Input::SetSuppressGameplayInput(false);
    if (world && player != criogenio::ecs::NULL_ENTITY) {
      if (auto *c = world->GetComponent<criogenio::Controller>(player))
        c->movement_frozen = false;
    }
    interactableStateFlags.clear();
    interactableEntityDataByKey.clear();
    mobEntityDataByEntity.clear();
    mobPrefabByEntity.clear();
    itemEventPairsInside.clear();
    itemEventCooldownUntilSec.clear();
    itemEventDispatchClockSec = 0.f;
    nearestInteractableIndex = -1;
    runtimeTriggers.clear();
    runtimeTriggerSeq = 1;
    criogenio::Terrain2D terrain = criogenio::TilemapLoader::LoadFromTMX(path);
    world->SetTerrain(std::make_unique<criogenio::Terrain2D>(std::move(terrain)));
    mapPath = path;
    triggerInsidePrevFrame.clear();
    rebuildTriggers();
    const std::string enteringBasename = mapBasenameFromPath(path);
    const auto persisted = persistedPickupsByBasename.find(enteringBasename);
    if (persisted != persistedPickupsByBasename.end()) {
      for (const PersistedPickup &p : persisted->second)
        SpawnPickupAt(*this, p.center_x, p.center_y, p.item_id, p.count, p.width, p.height);
    } else {
      SpawnTiledMapPrefabs(*this);
    }
    nearestPickupEntity = criogenio::ecs::NULL_ENTITY;
    interactHint.clear();
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
  if (!have && ter && ter->UsesGidMode() && ter->LogicalMapWidthTiles() > 0) {
    const float sx = static_cast<float>(ter->GridStepX());
    const float sy = static_cast<float>(ter->GridStepY());
    const auto &m = ter->tmxMeta;
    const float minPx = static_cast<float>(m.boundsMinTx) * sx;
    const float maxPx = static_cast<float>(m.boundsMaxTx) * sx;
    const float minPy = static_cast<float>(m.boundsMinTy) * sy;
    const float maxPy = static_cast<float>(m.boundsMaxTy) * sy;
    cx = (minPx + maxPx) * 0.5f;
    cy = (minPy + maxPy) * 0.5f;
    have = true;
  } else if (!have && ter && ter->LogicalMapWidthTiles() > 0) {
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
}

} // namespace subterra
