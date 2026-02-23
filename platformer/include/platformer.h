#pragma once

#include "engine.h"
#include "network/net_messages.h"
#include "render.h"
#include "systems.h"
#include "world.h"

static constexpr int PlatformerWidth = 1280;
static constexpr int PlatformerHeight = 720;
static constexpr uint16_t PlatformerNetPort = 7778;

/** Adds RigidBody, BoxCollider, Grounded, AnimatedSprite, AnimationState to player entities. */
class PlatformerInitSystem : public criogenio::ISystem {
public:
  criogenio::World& world;
  criogenio::AssetId playerAnimId;
  PlatformerInitSystem(criogenio::World& w, criogenio::AssetId animId)
      : world(w), playerAnimId(animId) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer& r) override;
};

/** Platformer movement: horizontal from Controller, jump when grounded. Uses RigidBody. */
class PlatformerMovementSystem : public criogenio::ISystem {
public:
  criogenio::World& world;
  float moveSpeed = 280.f;
  float jumpForce = 420.f;
  PlatformerMovementSystem(criogenio::World& w) : world(w) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer& r) override;
};

/** Updates camera target to follow the first player. Run after movement systems. */
class CameraFollowSystem : public criogenio::ISystem {
public:
  criogenio::World& world;
  CameraFollowSystem(criogenio::World& w) : world(w) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer& r) override;
};

class PlatformerEngine : public criogenio::Engine {
public:
  using criogenio::Engine::Engine;
  void OnFrame(float dt) override;
};
