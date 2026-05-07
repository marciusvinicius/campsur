#pragma once

#include <string>

namespace subterra {

struct SubterraSession;

/** Place mob sprites in a small grid around (cx, cy) in world pixels (center point). */
void SpawnMobsAround(SubterraSession &session, float cx, float cy, int count);
void SpawnMobsAround(SubterraSession &session, float cx, float cy, int count,
                     const std::string &mob_prefab_id);

/** Single mob centered at (cx, cy). */
void SpawnMobAt(SubterraSession &session, float cx, float cy);
void SpawnMobAt(SubterraSession &session, float cx, float cy, const std::string &mob_prefab_id);

/** World pickup at (cx, cy) center. Use `pickup_width` / `pickup_height` <= 0 for defaults. */
void SpawnPickupAt(SubterraSession &session, float cx, float cy, const std::string &item_id,
                   int count, float pickup_width = 0.f, float pickup_height = 0.f);

/** Spawn world pickups from TMX `spawn_prefab` objects (`tmxMeta.spawnPrefabs`). */
void SpawnTiledMapPrefabs(SubterraSession &session);

/** Spawn from entities with `WorldSpawnPrefab2D` + `Transform` (same rules as tiled spawn prefabs). */
void SpawnEntityPrefabMarkers(SubterraSession &session);

/**
 * Fill `mobPrefabByEntity` / `mobEntityDataByEntity` for mobs already in the world
 * (e.g. editor-placed `PrefabInstance` + `MobTag`) so brains and gameplay match spawned mobs.
 * Skips entities already registered and the session player.
 */
void RegisterAuthoringMobSessionState(SubterraSession &session);

} // namespace subterra
