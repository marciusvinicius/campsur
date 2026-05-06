#pragma once

#include "ecs_core.h"
#include "graphics_types.h"
#include "systems.h"
#include "world.h"

namespace subterra {

struct SubterraSession;

static constexpr int ScreenWidth = 1280;
static constexpr int ScreenHeight = 720;
/** Fallback grid when no TMX terrain is loaded. */
static constexpr int TileSize = 32;
static constexpr int MapWidthTiles = 40;
static constexpr int MapHeightTiles = 24;
static constexpr float PlayerMoveSpeed = 220.f;
/** Camera scale (1 = 1:1 world px). */
static constexpr float DefaultCameraZoom = 1.15f;

/** Set from `LoadPlayerAnimationDatabaseFromJson` before systems use it. */
extern int g_playerDrawW;
extern int g_playerDrawH;

/** Footprint ~Odin `PLAYER_BODY_RADIUS` (10): narrow box at feet for TMX `collision` layers. */
void SubterraApplyPlayerTileCollisionFootprint(criogenio::World &world,
                                               criogenio::ecs::EntityId player,
                                               float spriteW, float spriteH);

/** Clamp player transforms to the loaded terrain using `g_playerDrawW/H`. */
class MapBoundsSystem : public criogenio::ISystem {
public:
  criogenio::World &world;
  explicit MapBoundsSystem(criogenio::World &w) : world(w) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

class CameraFollowSystem : public criogenio::ISystem {
public:
  criogenio::World &world;
  SubterraSession &session;
  CameraFollowSystem(criogenio::World &w, SubterraSession &s) : world(w), session(s) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

/** E to pick up nearest item in range; 1–5 action bar, U use consumable; draws pickup markers. */
class PickupSystem : public criogenio::ISystem {
public:
  SubterraSession &session;
  explicit PickupSystem(SubterraSession &s) : session(s) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

/** Vitals + status effects tick (after movement/animation). */
class VitalsSystem : public criogenio::ISystem {
public:
  SubterraSession &session;
  explicit VitalsSystem(SubterraSession &s) : session(s) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

class MobBrainSystem : public criogenio::ISystem {
public:
  SubterraSession &session;
  explicit MobBrainSystem(SubterraSession &s) : session(s) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

class ItemEventDispatchSystem : public criogenio::ISystem {
public:
  SubterraSession &session;
  explicit ItemEventDispatchSystem(SubterraSession &s) : session(s) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

class ItemLightSyncSystem : public criogenio::ISystem {
public:
  SubterraSession &session;
  explicit ItemLightSyncSystem(SubterraSession &s) : session(s) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

} // namespace subterra
