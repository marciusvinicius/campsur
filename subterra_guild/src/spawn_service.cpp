#include "spawn_service.h"
#include "subterra_interactable_prefabs.h"
#include "animated_component.h"
#include "animation_database.h"
#include "components.h"
#include "subterra_components.h"
#include "subterra_session.h"
#include "terrain.h"
#include <cmath>

namespace subterra {

namespace {

constexpr float kMobGrid = 48.f;

} // namespace

void SpawnMobAt(SubterraSession &session, float centerX, float centerY) {
  if (!session.world || session.playerAnimId == criogenio::INVALID_ASSET_ID)
    return;
  criogenio::World &w = *session.world;
  const float sc = 0.85f;
  float dw = static_cast<float>(session.playerW) * sc;
  float dh = static_cast<float>(session.playerH) * sc;
  criogenio::ecs::EntityId e = w.CreateEntity("mob");
  w.AddComponent<criogenio::Transform>(e, centerX - dw * 0.5f, centerY - dh * 0.5f);
  auto *tr = w.GetComponent<criogenio::Transform>(e);
  if (tr) {
    tr->scale_x = sc;
    tr->scale_y = sc;
  }
  w.AddComponent<criogenio::AnimationState>(e);
  w.AddComponent<criogenio::AnimatedSprite>(e, session.playerAnimId);
  w.AddComponent<criogenio::Name>(e, "mob");
  w.AddComponent<MobTag>(e);
}

void SpawnMobsAround(SubterraSession &session, float cx, float cy, int count) {
  if (count <= 0)
    return;
  int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
  int n = 0;
  for (int row = 0; row < cols && n < count; ++row) {
    for (int col = 0; col < cols && n < count; ++col, ++n) {
      float ox = (static_cast<float>(col) - static_cast<float>(cols - 1) * 0.5f) * kMobGrid;
      float oy = (static_cast<float>(row) - static_cast<float>(cols - 1) * 0.5f) * kMobGrid;
      SpawnMobAt(session, cx + ox, cy + oy);
    }
  }
}

void SpawnPickupAt(SubterraSession &session, float centerX, float centerY,
                   const std::string &item_id, int count, float pickup_width, float pickup_height) {
  if (!session.world || item_id.empty() || count <= 0)
    return;
  criogenio::World &w = *session.world;
  WorldPickup wp(item_id, count);
  if (pickup_width > 0.5f)
    wp.width = pickup_width;
  if (pickup_height > 0.5f)
    wp.height = pickup_height;
  float halfW = wp.width * 0.5f;
  float halfH = wp.height * 0.5f;
  criogenio::ecs::EntityId e = w.CreateEntity("pickup");
  w.AddComponent<criogenio::Transform>(e, centerX - halfW, centerY - halfH);
  w.AddComponent<WorldPickup>(e, item_id, count, wp.width, wp.height);
}

void SpawnTiledMapPrefabs(SubterraSession &session) {
  if (!session.world)
    return;
  criogenio::Terrain2D *t = session.world->GetTerrain();
  if (!t)
    return;
  for (const criogenio::TiledSpawnPrefab &sp : t->tmxMeta.spawnPrefabs) {
    if (sp.prefabName.empty())
      continue;
    if (SubterraInteractablePrefabNameIsRegistered(sp.prefabName))
      continue;
    float cx = sp.x + sp.width * 0.5f;
    float cy = sp.y + sp.height * 0.5f;
    float pw = sp.width > 0.5f ? sp.width : 0.f;
    float ph = sp.height > 0.5f ? sp.height : 0.f;
    SpawnPickupAt(session, cx, cy, sp.prefabName, sp.quantity, pw, ph);
  }
}

} // namespace subterra
