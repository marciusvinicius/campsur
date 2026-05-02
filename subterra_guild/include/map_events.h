#pragma once

#include "terrain.h"
#include "tmx_metadata.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace subterra {

struct SubterraSession;

struct MapEventPayload {
  std::string event_id;
  std::string event_trigger;
  std::string event_type;
  /** Tiled object `type` / class (used for teleport / routing). */
  std::string object_type;
  std::string teleport_to;
  std::string spawn_point;
  float world_x = 0.f;
  float world_y = 0.f;
  float world_w = 0.f;
  float world_h = 0.f;
  bool is_point = false;
  int spawn_count = 0;
  bool manual = false;
};

struct MapEventTrigger {
  std::string storage_key;
  float x = 0.f;
  float y = 0.f;
  float w = 0.f;
  float h = 0.f;
  bool is_point = false;
  std::string event_id;
  std::string event_trigger;
  std::string event_type;
  std::string object_type;
  std::string teleport_to;
  std::string spawn_point;
  int spawn_count = 0;
};

class MapEventBus {
public:
  void addListener(std::function<void(const MapEventPayload &)> fn);
  void clear();
  void dispatch(const MapEventPayload &p) const;

private:
  std::vector<std::function<void(const MapEventPayload &)>> listeners_;
};

std::vector<MapEventTrigger> BuildMapEventTriggers(const criogenio::Terrain2D &terrain);

bool TriggerStringIsMonsterWave(const std::string &triggerLowerHint);
bool TriggerStringIsTeleport(const std::string &triggerLowerHint);

MapEventPayload MakePayloadFromTrigger(const MapEventTrigger &t, bool manual = false);

void DispatchMapEventDefaults(SubterraSession &session, const MapEventPayload &p);

/** Bit flags in `SubterraSession::interactableStateFlags` (per `InteractableStateKey`). */
struct InteractableState {
  static constexpr std::uint8_t Open = 1;
  static constexpr std::uint8_t Burning = 2;
};

std::string InteractableStateKey(const std::string &mapPath,
                                const criogenio::TiledInteractable &it);
/** Stored flags for this interactable key (0 when never toggled). */
std::uint8_t InteractableFlagsEffective(const SubterraSession &session,
                                        const criogenio::TiledInteractable &it);
void ApplyInteractableUse(SubterraSession &session, const criogenio::TiledInteractable &it);

/**
 * Resolve teleport spawn key to player center (world px, Tiled coords).
 * Matches reference: object id string, then event_id, then name; prefers markers
 * without teleport_to / target_level; offsets north when landing on an outgoing
 * teleporter so the player does not instantly warp back.
 */
bool FindSpawnCenter(const criogenio::TmxMapMetadata &meta, const std::string &name,
                     float &outCx, float &outCy);

std::string SpawnPointProperty(const std::vector<criogenio::TmxProperty> &props);

} // namespace subterra
