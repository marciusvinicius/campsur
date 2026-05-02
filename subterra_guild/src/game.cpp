#include "game.h"
#include "animated_component.h"
#include "components.h"
#include "graphics_types.h"
#include "subterra_components.h"
#include "subterra_session.h"
#include "terrain.h"
#include <cstdio>
#include <vector>

namespace subterra {

int g_playerDrawW = 64;
int g_playerDrawH = 64;

namespace {

void movementBoundsPx(criogenio::World &w, float &maxX, float &maxY) {
  criogenio::Terrain2D *t = w.GetTerrain();
  if (t && t->LogicalMapWidthTiles() > 0) {
    maxX = t->LogicalMapWidthTiles() * static_cast<float>(t->GridStepX()) -
           static_cast<float>(g_playerDrawW);
    maxY = t->LogicalMapHeightTiles() * static_cast<float>(t->GridStepY()) -
           static_cast<float>(g_playerDrawH);
  } else {
    maxX = (MapWidthTiles - 1) * static_cast<float>(TileSize) -
           static_cast<float>(g_playerDrawW);
    maxY = (MapHeightTiles - 1) * static_cast<float>(TileSize) -
           static_cast<float>(g_playerDrawH);
  }
  if (maxX < 0)
    maxX = 0;
  if (maxY < 0)
    maxY = 0;
}

} // namespace

void MapBoundsSystem::Update(float /*dt*/) {
  auto ids = world.GetEntitiesWith<PlayerTag, criogenio::Controller, criogenio::Transform,
                                    criogenio::AnimationState>();
  float maxX = 0, maxY = 0;
  movementBoundsPx(world, maxX, maxY);
  for (criogenio::ecs::EntityId id : ids) {
    auto *tr = world.GetComponent<criogenio::Transform>(id);
    if (!tr)
      continue;
    if (tr->x < 0)
      tr->x = 0;
    if (tr->y < 0)
      tr->y = 0;
    if (tr->x > maxX)
      tr->x = maxX;
    if (tr->y > maxY)
      tr->y = maxY;
  }
}

void MapBoundsSystem::Render(criogenio::Renderer &) {}

void CameraFollowSystem::Update(float /*dt*/) {
  criogenio::Camera2D *cam = world.GetActiveCamera();
  if (!cam)
    return;

  auto ids = world.GetEntitiesWith<PlayerTag, criogenio::Controller, criogenio::Transform,
                                    criogenio::AnimationState>();
  for (criogenio::ecs::EntityId id : ids) {
    auto *tr = world.GetComponent<criogenio::Transform>(id);
    if (!tr)
      continue;
    cam->target.x = tr->x + static_cast<float>(g_playerDrawW) * 0.5f;
    cam->target.y = tr->y + static_cast<float>(g_playerDrawH) * 0.5f;
    cam->zoom = DefaultCameraZoom;
    return;
  }
}

void CameraFollowSystem::Render(criogenio::Renderer &) {}

namespace {

bool aabbOverlap(float ax, float ay, float aw, float ah, float bx, float by, float bw,
                 float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

} // namespace

void PickupSystem::Update(float /*dt*/) {
  if (!session.world || session.player == criogenio::ecs::NULL_ENTITY)
    return;
  auto *inv = session.world->GetComponent<Inventory>(session.player);
  auto *ptr = session.world->GetComponent<criogenio::Transform>(session.player);
  if (!inv || !ptr)
    return;
  const float pw = static_cast<float>(session.playerW);
  const float ph = static_cast<float>(session.playerH);

  auto ids = session.world->GetEntitiesWith<WorldPickup, criogenio::Transform>();
  std::vector<criogenio::ecs::EntityId> to_remove;
  for (criogenio::ecs::EntityId id : ids) {
    auto *pk = session.world->GetComponent<WorldPickup>(id);
    auto *tr = session.world->GetComponent<criogenio::Transform>(id);
    if (!pk || !tr || pk->item_id.empty() || pk->count <= 0)
      continue;
    if (aabbOverlap(ptr->x, ptr->y, pw, ph, tr->x, tr->y, pk->width, pk->height)) {
      int left = inv->TryAdd(pk->item_id, pk->count);
      int taken = pk->count - left;
      if (taken > 0) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "Picked up %s x%d", pk->item_id.c_str(), taken);
        sessionLog(session, buf);
      }
      if (left <= 0)
        to_remove.push_back(id);
      else
        pk->count = left;
    }
  }
  for (criogenio::ecs::EntityId id : to_remove)
    session.world->DeleteEntity(id);
}

void PickupSystem::Render(criogenio::Renderer &renderer) {
  if (!session.world)
    return;
  auto ids = session.world->GetEntitiesWith<WorldPickup, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : ids) {
    auto *pk = session.world->GetComponent<WorldPickup>(id);
    auto *tr = session.world->GetComponent<criogenio::Transform>(id);
    if (!pk || !tr)
      continue;
    criogenio::Color fill{255, 200, 64, 90};
    criogenio::Color outline{255, 220, 100, 220};
    renderer.DrawRect(tr->x, tr->y, pk->width, pk->height, fill);
    renderer.DrawRectOutline(tr->x, tr->y, pk->width, pk->height, outline);
  }
}

} // namespace subterra
