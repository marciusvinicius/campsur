#pragma once

#include "animated_component.h"
#include "components.h"
#include "systems.h"
#include "world.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace criogenio {

/**
 * When non-null, `MovementSystem` asks this first for player movement axes (dx,dy in {-1,0,1}).
 * Return true if handled (skip built-in WASD for that entity). Used by Subterra for input_config.json bindings.
 */
using PlayerMovementAxisOverrideFn =
    bool (*)(World &world, ecs::EntityId id, float *outDx, float *outDy);
/** @deprecated Prefer SetWorldMovementInputProvider(world, ...). */
[[deprecated("Prefer SetWorldMovementInputProvider(world, ...)")]]
void SetPlayerMovementAxisOverride(PlayerMovementAxisOverrideFn fn);
/** @deprecated Prefer SetWorldMovementInputProvider(world, ...). */
[[deprecated("Prefer SetWorldMovementInputProvider(world, ...)")]]
PlayerMovementAxisOverrideFn GetPlayerMovementAxisOverride();

/** When non-null, used instead of LeftShift/RightShift for run speed multiplier. */
using PlayerRunHeldOverrideFn = bool (*)();
/** @deprecated Prefer SetWorldMovementInputProvider(world, ...). */
[[deprecated("Prefer SetWorldMovementInputProvider(world, ...)")]]
void SetPlayerRunHeldOverride(PlayerRunHeldOverrideFn fn);

/** Preferred instance-scoped movement input provider (world-local; replaces global hook patterns). */
using WorldMovementAxisProviderFn =
    bool (*)(void *user, World &world, ecs::EntityId id, float *outDx, float *outDy);
using WorldRunHeldProviderFn = bool (*)(void *user);
using WorldMovementBlockProviderFn = bool (*)(void *user, World &world, ecs::EntityId id,
                                              float rectLeft, float rectTop, float rectW,
                                              float rectH);
void SetWorldMovementInputProvider(World &world, WorldMovementAxisProviderFn axisFn,
                                   WorldRunHeldProviderFn runHeldFn = nullptr,
                                   void *user = nullptr);
void ClearWorldMovementInputProvider(World &world);
void SetWorldMovementBlockProvider(World &world, WorldMovementBlockProviderFn blockFn,
                                   void *user = nullptr);
void ClearWorldMovementBlockProvider(World &world);

struct CoreSystemPerfCounters {
  uint64_t movementSamples = 0;
  double movementAvgMs = 0.0;
  double movementMaxMs = 0.0;
  uint64_t animationSamples = 0;
  double animationAvgMs = 0.0;
  double animationMaxMs = 0.0;
};
const CoreSystemPerfCounters &GetCoreSystemPerfCounters();
void ResetCoreSystemPerfCounters();

class MovementSystem : public ISystem {
public:
  World &world;
  MovementSystem(World &w) : world(w) {}

  void Update(float dt) override;
  void Render(Renderer &renderer) override;
};

class MovementSystem3D : public ISystem {
public:
  World &world;
  MovementSystem3D(World &w) : world(w) {}

  void Update(float dt) override;
  void Render(Renderer &renderer) override;
};

class AIMovementSystem : public ISystem {
public:
  World &world;
  AIMovementSystem(World &w) : world(w) {}

  void Update(float dt) override;
  void Render(Renderer &renderer) override;
};

class AnimationSystem : public ISystem {
public:
  World &world;
  AnimationSystem(World &w) : world(w) {};
  std::string FacingToString(Direction d) const;
  std::string BuildClipKey(const AnimationState &st);
  void Update(float dt) override;
  void Render(Renderer &) override;
  void OnWorldLoaded(World &world);
};

class SpriteSystem : public ISystem {
public:
  World &world;
  SpriteSystem(World &w) : world(w) {};
  void Update(float dt) override;
  void Render(Renderer &) override;
};

class RenderSystem : public ISystem {
public:
  World &world;
  RenderSystem(World &w) : world(w) {}
  void Update(float) override;
  void Render(Renderer &renderer) override;
};

class GravitySystem : public ISystem {
public:
  World &world;
  GravitySystem(World &w) : world(w) {}
  void Update(float dt) override;
  void Render(Renderer &renderer) override;
};

/** Resolves RigidBody+BoxCollider entities against platform BoxColliders. Run after GravitySystem. */
class CollisionSystem : public ISystem {
public:
  World &world;
  CollisionSystem(World &w) : world(w) {}
  void Update(float dt) override;
  void Render(Renderer &renderer) override;
};

} // namespace criogenio
