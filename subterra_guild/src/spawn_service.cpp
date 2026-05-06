#include "spawn_service.h"
#include "subterra_interactable_prefabs.h"
#include "subterra_mob_prefabs.h"
#include "animated_component.h"
#include "animation_database.h"
#include "components.h"
#include "player_anim.h"
#include "subterra_components.h"
#include "subterra_session.h"
#include "terrain.h"
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace subterra {

namespace {

constexpr float kMobGrid = 48.f;
namespace fs = std::filesystem;
std::unordered_map<std::string, criogenio::AssetId> g_mobAnimByPrefab;

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool fileReadable(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  return static_cast<bool>(f);
}

std::string resolveMobAnimationPath(const SubterraSession &session, const std::string &rawPath) {
  if (rawPath.empty())
    return {};
  if (fileReadable(rawPath))
    return rawPath;
  const std::string prefixed = std::string("subterra_guild/") + rawPath;
  if (fileReadable(prefixed))
    return prefixed;
  if (!session.mapPath.empty()) {
    fs::path rel = fs::path(session.mapPath).parent_path() / rawPath;
    const std::string relStr = rel.lexically_normal().string();
    if (fileReadable(relStr))
      return relStr;
  }
  return rawPath;
}

criogenio::AssetId resolveMobAnimationId(SubterraSession &session,
                                         const SubterraMobPrefabDef &prefab) {
  if (prefab.animation_path.empty())
    return session.playerAnimId;
  const std::string key = lowerAscii(prefab.prefab_name);
  auto it = g_mobAnimByPrefab.find(key);
  if (it != g_mobAnimByPrefab.end())
    return it->second;
  const std::string path = resolveMobAnimationPath(session, prefab.animation_path);
  PlayerAnimConfig cfg{};
  const criogenio::AssetId animId = LoadPlayerAnimationDatabaseFromJson(path, &cfg);
  if (animId != criogenio::INVALID_ASSET_ID)
    g_mobAnimByPrefab[key] = animId;
  return animId;
}

} // namespace

void SpawnMobAt(SubterraSession &session, float centerX, float centerY) {
  SpawnMobAt(session, centerX, centerY, "skeleton");
}

void SpawnMobAt(SubterraSession &session, float centerX, float centerY,
                const std::string &mob_prefab_id) {
  if (!session.world || session.playerAnimId == criogenio::INVALID_ASSET_ID)
    return;
  criogenio::World &w = *session.world;
  SubterraMobPrefabDef def{};
  const bool hasPrefab = SubterraMobTryGetPrefabDef(mob_prefab_id, def);
  const float sc = 0.85f;
  float dw = static_cast<float>(session.playerW) * sc;
  float dh = static_cast<float>(session.playerH) * sc;
  criogenio::AssetId animId = session.playerAnimId;
  std::string displayName = "mob";
  if (hasPrefab) {
    if (!def.display_name.empty())
      displayName = def.display_name;
    if (def.size > 0) {
      dw = static_cast<float>(def.size) * sc;
      dh = static_cast<float>(def.size) * sc;
    }
    animId = resolveMobAnimationId(session, def);
    if (animId == criogenio::INVALID_ASSET_ID)
      animId = session.playerAnimId;
  }
  criogenio::ecs::EntityId e = w.CreateEntity("mob");
  w.AddComponent<criogenio::Transform>(e, centerX - dw * 0.5f, centerY - dh * 0.5f);
  auto *tr = w.GetComponent<criogenio::Transform>(e);
  if (tr) {
    tr->scale_x = sc;
    tr->scale_y = sc;
  }
  w.AddComponent<criogenio::AnimationState>(e);
  w.AddComponent<criogenio::AnimatedSprite>(e, animId);
  w.AddComponent<criogenio::Name>(e, displayName);
  w.AddComponent<MobTag>(e);
  auto &ai = w.AddComponent<criogenio::AIController>(e);
  ai.velocity = {90.f, 90.f};
  ai.brainState = criogenio::AIBrainState::ENEMY;
  ai.entityTarget = static_cast<int>(session.player);
  session.mobPrefabByEntity[e] = hasPrefab ? def.prefab_name : lowerAscii(mob_prefab_id);
  nlohmann::json state = nlohmann::json::object();
  if (hasPrefab && def.default_entity_data.is_object())
    state = def.default_entity_data;
  if (!state.contains("brain_type"))
    state["brain_type"] = hasPrefab ? "simple_chase_player" : "simple";
  state["base_scale_x"] = tr ? tr->scale_x : 1.f;
  state["base_scale_y"] = tr ? tr->scale_y : 1.f;
  state["prefab_name"] = hasPrefab ? def.prefab_name : lowerAscii(mob_prefab_id);
  session.mobEntityDataByEntity[e] = std::move(state);
}

void SpawnMobsAround(SubterraSession &session, float cx, float cy, int count) {
  SpawnMobsAround(session, cx, cy, count, "skeleton");
}

void SpawnMobsAround(SubterraSession &session, float cx, float cy, int count,
                     const std::string &mob_prefab_id) {
  if (count <= 0)
    return;
  int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
  int n = 0;
  for (int row = 0; row < cols && n < count; ++row) {
    for (int col = 0; col < cols && n < count; ++col, ++n) {
      float ox = (static_cast<float>(col) - static_cast<float>(cols - 1) * 0.5f) * kMobGrid;
      float oy = (static_cast<float>(row) - static_cast<float>(cols - 1) * 0.5f) * kMobGrid;
      SpawnMobAt(session, cx + ox, cy + oy, mob_prefab_id);
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
    if (SubterraMobPrefabNameIsRegistered(sp.prefabName)) {
      float cx = sp.x + sp.width * 0.5f;
      float cy = sp.y + sp.height * 0.5f;
      int count = sp.quantity > 0 ? sp.quantity : 1;
      SpawnMobsAround(session, cx, cy, count, sp.prefabName);
      continue;
    }
    float cx = sp.x + sp.width * 0.5f;
    float cy = sp.y + sp.height * 0.5f;
    float pw = sp.width > 0.5f ? sp.width : 0.f;
    float ph = sp.height > 0.5f ? sp.height : 0.f;
    SpawnPickupAt(session, cx, cy, sp.prefabName, sp.quantity, pw, ph);
  }
}

} // namespace subterra
