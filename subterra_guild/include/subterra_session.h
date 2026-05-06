#pragma once

#include "animation_database.h"
#include "ecs_core.h"
#include "delayed_command_queue.h"
#include "map_events.h"
#include "subterra_gameplay_actions.h"
#include "subterra_camera.h"
#include "subterra_day_night.h"
#include "subterra_input_config.h"
#include "subterra_status_effects.h"
#include "subterra_world_rules.h"
#include "tmx_metadata.h"
#include "world.h"
#include "json.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace criogenio {
class Engine;
}

namespace subterra {

/** Last-seen world pickups when leaving a map (restored on return; see Odin `persisted_spawned_items_by_level`). */
struct PersistedPickup {
  std::string item_id;
  int count = 1;
  float center_x = 0.f;
  float center_y = 0.f;
  float width = 28.f;
  float height = 28.f;
};

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
  /** Triggers added at runtime (console / game); merged in `rebuildTriggers`. Cleared on `loadMap`. */
  std::vector<MapEventTrigger> runtimeTriggers;
  int runtimeTriggerSeq = 1;
  std::unordered_set<std::string> triggerInsidePrevFrame;
  MapEventBus mapEvents;
  GameplayActionRegistry gameplay;
  criogenio::DelayedCommandQueue gameplayCommandQueue;
  int gameplayInputLockDepth = 0;

  /** Trigger hitbox overlay (MapEventSystem) and FPS line when toggled from the console. */
  bool debugOverlay = false;
  /** ImGui inventory panel visibility (toggle with Tab while playing). */
  bool showInventoryPanel = false;
  /** ImGui entity inspector (console `egui` / `entityinspector`). */
  bool showEntityInspector = false;
  /** Nearest `WorldPickup` within interact radius (for highlight + hint). */
  criogenio::ecs::EntityId nearestPickupEntity = criogenio::ecs::NULL_ENTITY;
  /** Cached from current terrain TMX (`rebuildTriggers`). */
  std::vector<criogenio::TiledInteractable> tiledInteractables;
  /** Index into `tiledInteractables` within use radius, or -1. */
  int nearestInteractableIndex = -1;
  /** Per `InteractableStateKey`: door/chest open, campfire burning, etc. */
  std::unordered_map<std::string, std::uint8_t> interactableStateFlags;
  /** Per interactable instance key: dynamic JSON state initialized from prefab `entity_data`. */
  std::unordered_map<std::string, nlohmann::json> interactableEntityDataByKey;
  /** Runtime mob state by entity id (prefab defaults + dynamic values). */
  std::unordered_map<criogenio::ecs::EntityId, nlohmann::json> mobEntityDataByEntity;
  /** Runtime mob prefab id by entity id. */
  std::unordered_map<criogenio::ecs::EntityId, std::string> mobPrefabByEntity;
  /** Item event dispatcher overlap cache (`source|event|target`). */
  std::unordered_set<std::string> itemEventPairsInside;
  /** Item event dispatcher cooldown expiry (seconds, session clock). */
  std::unordered_map<std::string, float> itemEventCooldownUntilSec;
  float itemEventDispatchClockSec = 0.f;
  /** Key = TMX filename (e.g. `City.tmx`). If present, `loadMap` restores pickups instead of TMX prefab spawn. */
  std::unordered_map<std::string, std::vector<PersistedPickup>> persistedPickupsByBasename;
  /** e.g. `[E] Pick up Torch x2` when something is in range. */
  std::string interactHint;

  SubterraDayNight dayNight;
  SubterraWorldRules worldRules;
  SubterraStatusRegistry statusRegistry;
  SubterraCameraBundle camera;
  SubterraInputRuntime input;
  /** Paths successfully used for hot reload (empty if load failed). */
  std::string worldConfigPath;
  std::string inputConfigPath;
  float inputHotReloadAccum = 0.f;
  /** From `world_config.json` → `debug.debug_mode` (optional auto-debug). */
  bool debugModeFromConfig = false;
  /** Previous frame run key for camera zoom edges. */
  bool runHeldPrev = false;

  void rebuildTriggers();
  /** Append a trigger zone; assigns `storage_key` if empty (`rt_N`). Refreshes `triggers`. */
  std::string addRuntimeTrigger(MapEventTrigger t);
  void clearRuntimeTriggers();
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
