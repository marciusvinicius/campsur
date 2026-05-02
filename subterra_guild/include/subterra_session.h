#pragma once

#include "animation_database.h"
#include "ecs_core.h"
#include "map_events.h"
#include "world.h"
#include <string>

namespace criogenio {
class Engine;
}

namespace subterra {

struct SubterraSession {
  criogenio::World *world = nullptr;
  criogenio::Engine *engine = nullptr;
  criogenio::ecs::EntityId player = criogenio::ecs::NULL_ENTITY;
  std::string mapPath;
  int playerW = 64;
  int playerH = 64;
  /** Shared animation DB id for player and spawned mobs. */
  criogenio::AssetId playerAnimId = criogenio::INVALID_ASSET_ID;
  float fpsSmooth = 0.f;

  std::vector<MapEventTrigger> triggers;
  std::unordered_set<std::string> triggerInsidePrevFrame;
  MapEventBus mapEvents;

  /** Trigger hitbox overlay (MapEventSystem) and FPS line when toggled from the console. */
  bool debugOverlay = false;

  void rebuildTriggers();
  /** Remove mobs and world pickups (not the player). Call after changing maps. */
  void destroyTransientEntities();
  bool loadMap(const std::string &path, std::string &errOut);
  /** Empty name: try player_start_position then map center. */
  void placePlayerAtSpawn(const std::string &spawnNameOrEmpty);

  bool fireTiledEventByKey(const std::string &key);
  void emitManual(const std::string &eventId, const std::string &trigger,
                  const std::string &eventType);
};

/** Append a line to the engine debug console if session.engine is set. */
void sessionLog(SubterraSession &session, const std::string &line);

} // namespace subterra
