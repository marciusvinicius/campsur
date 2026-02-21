#pragma once

#include "animated_component.h"
#include "components.h"
#include "systems.h"
#include "world.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace criogenio {

class MovementSystem : public ISystem {
public:
  World &world;
  MovementSystem(World &w) : world(w) {}

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
