#pragma once

#include "components.h"
#include "engine.h"
#include "graphics_types.h"
#include "network/net_messages.h"
#include "systems.h"
#include "world.h"

namespace tiled {

static constexpr int ScreenWidth = 1280;
static constexpr int ScreenHeight = 720;
static constexpr int TileSize = 32;
static constexpr int MapWidthTiles = 40;
static constexpr int MapHeightTiles = 24;
static constexpr float PlayerMoveSpeed = 180.f;
static constexpr float PlayerRadius = 14.f;
static constexpr uint16_t DefaultPort = 7777;

/** Renders a simple tiled floor (checkerboard) in world space. */
class TileMapSystem : public criogenio::ISystem {
public:
  criogenio::World &world;
  explicit TileMapSystem(criogenio::World &w) : world(w) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

/** Applies Controller.velocity to Transform (used on server for networked players). */
class VelocityMovementSystem : public criogenio::ISystem {
public:
  criogenio::World &world;
  explicit VelocityMovementSystem(criogenio::World &w) : world(w) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

/** Draws entities with NetReplicated + Transform as circles (multiplayer players). */
class PlayerRenderSystem : public criogenio::ISystem {
public:
  criogenio::World &world;
  explicit PlayerRenderSystem(criogenio::World &w) : world(w) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

/** Engine that sends client input each frame when in client mode. */
class GameEngine : public criogenio::Engine {
public:
  using criogenio::Engine::Engine;
  void OnFrame(float dt) override;
};

}  // namespace tiled
