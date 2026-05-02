#include "game.h"
#include "animated_component.h"
#include "components.h"
#include "graphics_types.h"
#include "input.h"
#include "item_catalog.h"
#include "keys.h"
#include "map_events.h"
#include "subterra_interactable_prefabs.h"
#include "subterra_item_consumable.h"
#include "subterra_player_vitals.h"
#include "subterra_vitals_tick.h"
#include "subterra_components.h"
#include "subterra_loadout.h"
#include "subterra_camera.h"
#include "subterra_input_config.h"
#include "subterra_session.h"
#include "terrain.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace subterra {

int g_playerDrawW = 64;
int g_playerDrawH = 64;

namespace {

/** Matches reference ~20px body width for 10px radius; short box anchored at sprite feet. */
constexpr float kTileCollisionBodyW = 20.f;
constexpr float kTileCollisionBodyH = 14.f;


void movementBoundsPx(criogenio::World &w, float &minX, float &minY, float &maxX,
                     float &maxY) {
  criogenio::Terrain2D *t = w.GetTerrain();
  if (t && t->UsesGidMode() && t->LogicalMapWidthTiles() > 0) {
    const float sx = static_cast<float>(t->GridStepX());
    const float sy = static_cast<float>(t->GridStepY());
    const auto &m = t->tmxMeta;
    minX = static_cast<float>(m.boundsMinTx) * sx;
    minY = static_cast<float>(m.boundsMinTy) * sy;
    maxX = static_cast<float>(m.boundsMaxTx) * sx - static_cast<float>(g_playerDrawW);
    maxY = static_cast<float>(m.boundsMaxTy) * sy - static_cast<float>(g_playerDrawH);
  } else if (t && t->LogicalMapWidthTiles() > 0) {
    minX = 0.f;
    minY = 0.f;
    maxX = t->LogicalMapWidthTiles() * static_cast<float>(t->GridStepX()) -
           static_cast<float>(g_playerDrawW);
    maxY = t->LogicalMapHeightTiles() * static_cast<float>(t->GridStepY()) -
           static_cast<float>(g_playerDrawH);
  } else {
    minX = minY = 0.f;
    maxX = (MapWidthTiles - 1) * static_cast<float>(TileSize) -
           static_cast<float>(g_playerDrawW);
    maxY = (MapHeightTiles - 1) * static_cast<float>(TileSize) -
           static_cast<float>(g_playerDrawH);
  }
  if (maxX < minX)
    maxX = minX;
  if (maxY < minY)
    maxY = minY;
}

constexpr float kInteractPickupRadiusPx = 64.f;
/** Use radius for tiled interactables (reference ~28px; scaled for 64px-tile feel). */
constexpr float kInteractableUseRadiusPx = 44.f;

float dist2PointToRectCenter(float px, float py, float rx, float ry, float rw, float rh) {
  float cx = rx + rw * 0.5f;
  float cy = ry + rh * 0.5f;
  float dx = px - cx;
  float dy = py - cy;
  return dx * dx + dy * dy;
}

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

int inventoryCountFold(const criogenio::Inventory &inv, const std::string &want) {
  std::string wantL = lowerAscii(want);
  int total = 0;
  for (const auto &s : inv.Slots()) {
    if (s.item_id.empty() || s.count <= 0)
      continue;
    if (lowerAscii(s.item_id) == wantL)
      total += s.count;
  }
  return total;
}

/** First inventory key matching `want` case-insensitively (for `TryRemove`). */
std::string resolveInventoryKey(const criogenio::Inventory &inv, const std::string &want) {
  std::string wantL = lowerAscii(want);
  for (const auto &s : inv.Slots()) {
    if (s.item_id.empty() || s.count <= 0)
      continue;
    if (lowerAscii(s.item_id) == wantL)
      return s.item_id;
  }
  return {};
}

static void tiledInteractableCenter(const criogenio::TiledInteractable &it, float &cx,
                                   float &cy) {
  if (it.is_point) {
    cx = it.x;
    cy = it.y;
  } else {
    cx = it.x + it.w * 0.5f;
    cy = it.y + it.h * 0.5f;
  }
}

static std::string humanizeInteractableKind(const std::string &kind) {
  if (kind.empty())
    return "Object";
  std::string s = kind;
  s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
  return s;
}

void sanitizeActionBar(SubterraLoadout *load, const criogenio::Inventory *inv) {
  if (!load || !inv)
    return;
  for (auto &ref : load->action_bar) {
    if (ref.empty())
      continue;
    if (inventoryCountFold(*inv, ref) <= 0)
      ref.clear();
  }
}

} // namespace

void SubterraApplyPlayerTileCollisionFootprint(criogenio::World &world,
                                               criogenio::ecs::EntityId player,
                                               float spriteW, float spriteH) {
  auto *ctrl = world.GetComponent<criogenio::Controller>(player);
  if (!ctrl)
    return;
  ctrl->tile_collision_w = kTileCollisionBodyW;
  ctrl->tile_collision_h = kTileCollisionBodyH;
  ctrl->tile_collision_offset_x =
      std::max(0.f, (spriteW - kTileCollisionBodyW) * 0.5f);
  ctrl->tile_collision_offset_y =
      std::max(0.f, spriteH - kTileCollisionBodyH);
}

void MapBoundsSystem::Update(float /*dt*/) {
  auto ids = world.GetEntitiesWith<PlayerTag, criogenio::Controller, criogenio::Transform,
                                    criogenio::AnimationState>();
  float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
  movementBoundsPx(world, minX, minY, maxX, maxY);
  for (criogenio::ecs::EntityId id : ids) {
    auto *tr = world.GetComponent<criogenio::Transform>(id);
    if (!tr)
      continue;
    if (tr->x < minX)
      tr->x = minX;
    if (tr->y < minY)
      tr->y = minY;
    if (tr->x > maxX)
      tr->x = maxX;
    if (tr->y > maxY)
      tr->y = maxY;
  }
}

void MapBoundsSystem::Render(criogenio::Renderer &) {}

void CameraFollowSystem::Update(float dt) {
  criogenio::Camera2D *cam = world.GetActiveCamera();
  if (!cam)
    return;

  const bool runHeld = SubterraInputActionDown(session, "run");
  if (runHeld != session.runHeldPrev) {
    if (runHeld)
      SubterraCameraNotifyTrigger(session, "on_run_start");
    else
      SubterraCameraNotifyTrigger(session, "on_run_stop");
  }
  session.runHeldPrev = runHeld;
  SubterraCameraTick(session, dt, runHeld);

  auto ids = world.GetEntitiesWith<PlayerTag, criogenio::Controller, criogenio::Transform,
                                    criogenio::AnimationState>();
  for (criogenio::ecs::EntityId id : ids) {
    auto *tr = world.GetComponent<criogenio::Transform>(id);
    if (!tr)
      continue;
    const float cx = tr->x + static_cast<float>(g_playerDrawW) * 0.5f;
    const float cy = tr->y + static_cast<float>(g_playerDrawH) * 0.5f;
    SubterraCameraApplyToView(session, cam, cx, cy);
    return;
  }
  cam->zoom = session.camera.cfg.zoom > 1e-4f ? session.camera.cfg.zoom : DefaultCameraZoom;
}

void CameraFollowSystem::Render(criogenio::Renderer &) {}

void PickupSystem::Update(float /*dt*/) {
  session.nearestPickupEntity = criogenio::ecs::NULL_ENTITY;
  session.nearestInteractableIndex = -1;
  session.interactHint.clear();

  if (!session.world || session.player == criogenio::ecs::NULL_ENTITY)
    return;
  if (auto *vdead = session.world->GetComponent<PlayerVitals>(session.player);
      vdead && vdead->dead)
    return;
  auto *inv = session.world->GetComponent<Inventory>(session.player);
  auto *ptr = session.world->GetComponent<criogenio::Transform>(session.player);
  auto *load = session.world->GetComponent<SubterraLoadout>(session.player);
  if (!inv || !ptr)
    return;
  if (load)
    sanitizeActionBar(load, inv);

  const float pw = static_cast<float>(session.playerW);
  const float ph = static_cast<float>(session.playerH);
  const float pcx = ptr->x + pw * 0.5f;
  const float pcy = ptr->y + ph * 0.5f;
  const float r2 = kInteractPickupRadiusPx * kInteractPickupRadiusPx;

  criogenio::ecs::EntityId best = criogenio::ecs::NULL_ENTITY;
  float bestD2 = r2;

  auto ids = session.world->GetEntitiesWith<WorldPickup, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : ids) {
    auto *pk = session.world->GetComponent<WorldPickup>(id);
    auto *tr = session.world->GetComponent<criogenio::Transform>(id);
    if (!pk || !tr || pk->item_id.empty() || pk->count <= 0)
      continue;
    if (SubterraInteractablePrefabNameIsRegistered(pk->item_id))
      continue;
    float d2 = dist2PointToRectCenter(pcx, pcy, tr->x, tr->y, pk->width, pk->height);
    if (d2 <= bestD2) {
      bestD2 = d2;
      best = id;
    }
  }

  session.nearestPickupEntity = best;

  const float useR2 = kInteractableUseRadiusPx * kInteractableUseRadiusPx;
  int bestInteract = -1;
  float bestInteractD2 = useR2;
  for (int i = 0; i < static_cast<int>(session.tiledInteractables.size()); ++i) {
    const criogenio::TiledInteractable &it = session.tiledInteractables[static_cast<size_t>(i)];
    float icx = 0.f, icy = 0.f;
    tiledInteractableCenter(it, icx, icy);
    float dx = pcx - icx;
    float dy = pcy - icy;
    float d2 = dx * dx + dy * dy;
    if (d2 <= bestInteractD2) {
      bestInteractD2 = d2;
      bestInteract = i;
    }
  }
  session.nearestInteractableIndex = bestInteract;

  if (best != criogenio::ecs::NULL_ENTITY) {
    auto *pk = session.world->GetComponent<WorldPickup>(best);
    if (pk) {
      char hint[192];
      std::snprintf(hint, sizeof hint, "[E] Pick up %s x%d",
                    criogenio::ItemCatalog::DisplayName(pk->item_id).c_str(), pk->count);
      session.interactHint = hint;
    }
  } else if (bestInteract >= 0) {
    const criogenio::TiledInteractable &it =
        session.tiledInteractables[static_cast<size_t>(bestInteract)];
    char hint[160];
    std::snprintf(hint, sizeof hint, "[E] Interact (%s)",
                  humanizeInteractableKind(it.interactable_type).c_str());
    session.interactHint = hint;
  }

  if (!criogenio::Input::IsGameplayInputSuppressed() && load) {
    if (criogenio::Input::IsKeyPressed(static_cast<int>(criogenio::Key::Num1)))
      load->active_action_slot = 0;
    if (criogenio::Input::IsKeyPressed(static_cast<int>(criogenio::Key::Num2)))
      load->active_action_slot = 1;
    if (criogenio::Input::IsKeyPressed(static_cast<int>(criogenio::Key::Num3)))
      load->active_action_slot = 2;
    if (criogenio::Input::IsKeyPressed(static_cast<int>(criogenio::Key::Num4)))
      load->active_action_slot = 3;
    if (criogenio::Input::IsKeyPressed(static_cast<int>(criogenio::Key::Num5)))
      load->active_action_slot = 4;
    if (load->active_action_slot < 0)
      load->active_action_slot = 0;
    if (load->active_action_slot >= kActionBarSlots)
      load->active_action_slot = kActionBarSlots - 1;

    if (SubterraInputActionPressed(session, "use_item")) {
      int si = load->active_action_slot;
      if (si >= 0 && si < kActionBarSlots) {
        const std::string &keyItem = load->action_bar[static_cast<size_t>(si)];
        if (!keyItem.empty() && inventoryCountFold(*inv, keyItem) > 0) {
          float rh = 0.f, rf = 0.f;
          const bool jsonConsume = SubterraItemConsumableLookup(keyItem, rh, rf);
          const bool catConsume = criogenio::ItemCatalog::IsConsumable(keyItem);
          if (jsonConsume || catConsume) {
            std::string invKey = resolveInventoryKey(*inv, keyItem);
            if (!invKey.empty() && inv->TryRemove(invKey, 1) >= 1) {
              if (auto *vit = session.world->GetComponent<PlayerVitals>(session.player)) {
                if (rh > 0.f)
                  vit->health = std::min(vit->health_max, vit->health + rh);
                if (rf > 0.f)
                  vit->food_satiety = std::min(vit->food_satiety_max, vit->food_satiety + rf);
                if (catConsume && !jsonConsume)
                  vit->health = std::min(vit->health_max, vit->health + 25.f);
              }
              char buf[120];
              std::snprintf(buf, sizeof buf, "Used %s",
                            criogenio::ItemCatalog::DisplayName(invKey).c_str());
              sessionLog(session, buf);
              load->ClearRefsForItem(invKey);
            }
          }
        }
      }
    }

    if (SubterraInputActionPressed(session, "interact")) {
      if (best != criogenio::ecs::NULL_ENTITY) {
        auto *pk = session.world->GetComponent<WorldPickup>(best);
        if (pk) {
          int left = inv->TryAdd(pk->item_id, pk->count);
          int taken = pk->count - left;
          if (taken > 0) {
            char buf[160];
            std::snprintf(buf, sizeof buf, "Picked up %s x%d", pk->item_id.c_str(), taken);
            sessionLog(session, buf);
          }
          if (left <= 0)
            session.world->DeleteEntity(best);
          else
            pk->count = left;
        }
      } else if (session.nearestInteractableIndex >= 0) {
        ApplyInteractableUse(
            session,
            session.tiledInteractables[static_cast<size_t>(session.nearestInteractableIndex)]);
      }
    }
  }
}

void PickupSystem::Render(criogenio::Renderer &renderer) {
  if (!session.world)
    return;
  if (session.nearestPickupEntity == criogenio::ecs::NULL_ENTITY &&
      session.nearestInteractableIndex >= 0) {
    const criogenio::TiledInteractable &it =
        session.tiledInteractables[static_cast<size_t>(session.nearestInteractableIndex)];
    float ix = it.x, iy = it.y, iw = it.w, ih = it.h;
    if (it.is_point) {
      iw = 16.f;
      ih = 16.f;
      ix -= iw * 0.5f;
      iy -= ih * 0.5f;
    }
    criogenio::Color outline = {120, 220, 255, 220};
    renderer.DrawRectOutline(ix, iy, iw, ih, outline);
  }
  auto ids = session.world->GetEntitiesWith<WorldPickup, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : ids) {
    auto *pk = session.world->GetComponent<WorldPickup>(id);
    auto *tr = session.world->GetComponent<criogenio::Transform>(id);
    if (!pk || !tr)
      continue;
    if (SubterraInteractablePrefabNameIsRegistered(pk->item_id))
      continue;
    const bool highlight = (id == session.nearestPickupEntity);
    criogenio::Color fill =
        highlight ? criogenio::Color{255, 220, 100, 140} : criogenio::Color{255, 200, 64, 90};
    criogenio::Color outline =
        highlight ? criogenio::Color{255, 255, 160, 255} : criogenio::Color{255, 220, 100, 220};
    renderer.DrawRect(tr->x, tr->y, pk->width, pk->height, fill);
    renderer.DrawRectOutline(tr->x, tr->y, pk->width, pk->height, outline);
  }
}

void VitalsSystem::Update(float dt) {
  if (!session.world)
    return;
  SubterraTickVitalsAndStatus(session, *session.world, dt);
}

void VitalsSystem::Render(criogenio::Renderer &) {}

} // namespace subterra
