#pragma once

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
  explicit CameraFollowSystem(criogenio::World &w) : world(w) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

/** Overlap test against player; draws pickup markers. */
class PickupSystem : public criogenio::ISystem {
public:
  SubterraSession &session;
  explicit PickupSystem(SubterraSession &s) : session(s) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

} // namespace subterra
