/**
 * Integration test for SubterraSession trigger/interactable deduplication.
 *
 * Exercises the three-step dedup contract implemented in SubterraSession::rebuildTriggers():
 *   1. TMX-derived triggers with a storage_key matching an ECS entity are removed.
 *   2. ECS triggers are appended after the filtered TMX set.
 *   3. TMX-only triggers (no matching ECS entity) survive untouched.
 *
 * Also tests CollectMapEventTriggersFromEntities and CollectInteractablesFromEntities
 * independently to verify the ECS → runtime struct mapping.
 */
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "components.h"
#include "map_authoring_components.h"
#include "subterra_level_ecs.h"
#include "map_events.h"
#include "tmx_metadata.h"
#include "terrain.h"
#include "world.h"

using namespace criogenio;
using namespace subterra;

// ---------------------------------------------------------------------------
// Helper: replicate the dedup algorithm from SubterraSession::rebuildTriggers
// ---------------------------------------------------------------------------
static std::vector<MapEventTrigger> dedup(
    std::vector<MapEventTrigger> tmxTriggers,
    const std::vector<MapEventTrigger> &ecsTriggers)
{
  std::unordered_set<std::string> ecsKeys;
  for (const auto &e : ecsTriggers) {
    if (!e.storage_key.empty())
      ecsKeys.insert(e.storage_key);
  }
  if (!ecsKeys.empty()) {
    tmxTriggers.erase(
        std::remove_if(tmxTriggers.begin(), tmxTriggers.end(),
                       [&](const MapEventTrigger &t) {
                         return !t.storage_key.empty() &&
                                ecsKeys.count(t.storage_key) > 0;
                       }),
        tmxTriggers.end());
  }
  tmxTriggers.insert(tmxTriggers.end(), ecsTriggers.begin(), ecsTriggers.end());
  return tmxTriggers;
}

static std::vector<TiledInteractable> dedupInteractables(
    std::vector<TiledInteractable> tmxOnes,
    const std::vector<TiledInteractable> &ecsOnes)
{
  std::unordered_set<int> ecsIds;
  for (const auto &i : ecsOnes) {
    if (i.tiled_object_id != 0)
      ecsIds.insert(i.tiled_object_id);
  }
  if (!ecsIds.empty()) {
    tmxOnes.erase(
        std::remove_if(tmxOnes.begin(), tmxOnes.end(),
                       [&](const TiledInteractable &ti) {
                         return ti.tiled_object_id != 0 &&
                                ecsIds.count(ti.tiled_object_id) > 0;
                       }),
        tmxOnes.end());
  }
  tmxOnes.insert(tmxOnes.end(), ecsOnes.begin(), ecsOnes.end());
  return tmxOnes;
}

// ---------------------------------------------------------------------------
// Helper: build a stub tmx trigger for testing
// ---------------------------------------------------------------------------
static MapEventTrigger makeTmxTrigger(const std::string &key, const std::string &eventId) {
  MapEventTrigger t;
  t.storage_key = key;
  t.event_id = eventId;
  t.x = 100.f; t.y = 200.f; t.w = 64.f; t.h = 64.f;
  return t;
}

// ---------------------------------------------------------------------------
// Test 1: CollectMapEventTriggersFromEntities maps component fields correctly
// ---------------------------------------------------------------------------
static void test_collect_map_event_triggers() {
  std::cout << "  test_collect_map_event_triggers... ";

  World world;
  const ecs::EntityId e = world.CreateEntity("zone1");
  world.AddComponent<Transform>(e, 50.f, 80.f);
  auto &z = world.AddComponent<MapEventZone2D>(e);
  z.storage_key = "spawn_cave_A";
  z.event_id = "monster_wave_1";
  z.event_trigger = "spawn_monsters";
  z.event_type = "on_collider";
  z.width = 96.f;
  z.height = 48.f;
  z.activated = true;

  std::vector<MapEventTrigger> out;
  CollectMapEventTriggersFromEntities(world, out);

  assert(out.size() == 1);
  assert(out[0].storage_key == "spawn_cave_A");
  assert(out[0].event_id == "monster_wave_1");
  assert(out[0].event_trigger == "spawn_monsters");
  assert(out[0].w == 96.f);
  assert(out[0].h == 48.f);
  assert(!out[0].is_point);

  std::cout << "ok\n";
}

// ---------------------------------------------------------------------------
// Test 2: Inactive zone (activated = false) is not collected
// ---------------------------------------------------------------------------
static void test_inactive_zone_not_collected() {
  std::cout << "  test_inactive_zone_not_collected... ";

  World world;
  const ecs::EntityId e = world.CreateEntity("inactive");
  world.AddComponent<Transform>(e, 0.f, 0.f);
  auto &z = world.AddComponent<MapEventZone2D>(e);
  z.storage_key = "should_be_skipped";
  z.event_trigger = "teleport_to";
  z.activated = false;  // explicitly off

  std::vector<MapEventTrigger> out;
  CollectMapEventTriggersFromEntities(world, out);
  assert(out.empty());

  std::cout << "ok\n";
}

// ---------------------------------------------------------------------------
// Test 3: Dedup — ECS trigger overrides TMX trigger with same storage_key
// ---------------------------------------------------------------------------
static void test_dedup_ecs_overrides_tmx() {
  std::cout << "  test_dedup_ecs_overrides_tmx... ";

  // Build "TMX" set: two triggers, one shared key
  std::vector<MapEventTrigger> tmx;
  tmx.push_back(makeTmxTrigger("spawn_cave_A", "tmx_evt_1")); // will be overridden
  tmx.push_back(makeTmxTrigger("spawn_cave_B", "tmx_evt_2")); // unique – must survive

  // Build ECS set: overrides spawn_cave_A
  std::vector<MapEventTrigger> ecs;
  MapEventTrigger ecsA;
  ecsA.storage_key = "spawn_cave_A";
  ecsA.event_id = "ecs_evt_1"; // different from TMX value
  ecs.push_back(ecsA);

  auto result = dedup(tmx, ecs);

  assert(result.size() == 2);

  // spawn_cave_B (TMX-only) must be present
  auto itB = std::find_if(result.begin(), result.end(),
                          [](const MapEventTrigger &t) { return t.storage_key == "spawn_cave_B"; });
  assert(itB != result.end());
  assert(itB->event_id == "tmx_evt_2");

  // spawn_cave_A must be the ECS version
  auto itA = std::find_if(result.begin(), result.end(),
                          [](const MapEventTrigger &t) { return t.storage_key == "spawn_cave_A"; });
  assert(itA != result.end());
  assert(itA->event_id == "ecs_evt_1");

  std::cout << "ok\n";
}

// ---------------------------------------------------------------------------
// Test 4: Dedup — no ECS triggers, all TMX triggers survive
// ---------------------------------------------------------------------------
static void test_dedup_no_ecs_keeps_all_tmx() {
  std::cout << "  test_dedup_no_ecs_keeps_all_tmx... ";

  std::vector<MapEventTrigger> tmx;
  tmx.push_back(makeTmxTrigger("zone_A", "evt_A"));
  tmx.push_back(makeTmxTrigger("zone_B", "evt_B"));
  tmx.push_back(makeTmxTrigger("zone_C", "evt_C"));

  auto result = dedup(tmx, {});
  assert(result.size() == 3);

  std::cout << "ok\n";
}

// ---------------------------------------------------------------------------
// Test 5: CollectInteractablesFromEntities maps component fields correctly
// ---------------------------------------------------------------------------
static void test_collect_interactables() {
  std::cout << "  test_collect_interactables... ";

  World world;
  const ecs::EntityId e = world.CreateEntity("door_lever");
  world.AddComponent<Transform>(e, 300.f, 400.f);
  auto &iz = world.AddComponent<InteractableZone2D>(e);
  iz.interactable_type = "lever";
  iz.tiled_object_id = 42;
  iz.width = 32.f;
  iz.height = 32.f;
  iz.properties_json = R"({"opens_door":"light_door_1"})";

  std::vector<TiledInteractable> out;
  CollectInteractablesFromEntities(world, out);

  assert(out.size() == 1);
  assert(out[0].interactable_type == "lever");
  assert(out[0].tiled_object_id == 42);
  assert(out[0].w == 32.f);
  assert(!out[0].properties.empty());
  assert(out[0].properties[0].name == "opens_door");
  assert(out[0].properties[0].value == "light_door_1");

  std::cout << "ok\n";
}

// ---------------------------------------------------------------------------
// Test 6: Interactable dedup — ECS wins over TMX entry with same object id
// ---------------------------------------------------------------------------
static void test_dedup_interactables() {
  std::cout << "  test_dedup_interactables... ";

  std::vector<TiledInteractable> tmx;
  TiledInteractable tmxA; tmxA.tiled_object_id = 42; tmxA.interactable_type = "door_tmx";
  TiledInteractable tmxB; tmxB.tiled_object_id = 99; tmxB.interactable_type = "chest_tmx";
  tmx.push_back(tmxA);
  tmx.push_back(tmxB);

  std::vector<TiledInteractable> ecs;
  TiledInteractable ecsA; ecsA.tiled_object_id = 42; ecsA.interactable_type = "door_ecs";
  ecs.push_back(ecsA);

  auto result = dedupInteractables(tmx, ecs);
  assert(result.size() == 2);

  auto itA = std::find_if(result.begin(), result.end(),
                          [](const TiledInteractable &i) { return i.tiled_object_id == 42; });
  assert(itA != result.end());
  assert(itA->interactable_type == "door_ecs"); // ECS wins

  auto itB = std::find_if(result.begin(), result.end(),
                          [](const TiledInteractable &i) { return i.tiled_object_id == 99; });
  assert(itB != result.end());
  assert(itB->interactable_type == "chest_tmx"); // TMX-only survives

  std::cout << "ok\n";
}

// ---------------------------------------------------------------------------
// Test 7: Point zone gets expanded to a minimum pick area
// ---------------------------------------------------------------------------
static void test_point_zone_expanded() {
  std::cout << "  test_point_zone_expanded... ";

  World world;
  const ecs::EntityId e = world.CreateEntity("point_zone");
  world.AddComponent<Transform>(e, 200.f, 300.f);
  auto &z = world.AddComponent<MapEventZone2D>(e);
  z.storage_key = "pt_spawn";
  z.event_trigger = "teleport_to";
  z.point = true; // zero-size point
  z.activated = true;

  std::vector<MapEventTrigger> out;
  CollectMapEventTriggersFromEntities(world, out);

  assert(out.size() == 1);
  assert(out[0].is_point);
  // Must have been expanded so w and h >= some positive value
  assert(out[0].w > 0.f);
  assert(out[0].h > 0.f);

  std::cout << "ok\n";
}

int main() {
  std::cout << "=== Subterra Dedup Integration Test ===\n";

  test_collect_map_event_triggers();
  test_inactive_zone_not_collected();
  test_dedup_ecs_overrides_tmx();
  test_dedup_no_ecs_keeps_all_tmx();
  test_collect_interactables();
  test_dedup_interactables();
  test_point_zone_expanded();

  std::cout << "All tests passed.\n";
  return 0;
}
