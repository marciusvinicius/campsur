#pragma once

#include "animated_component.h"
#include "systems.h"
#include "world2.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace criogenio {

class MovementSystem : public ISystem {
public:
  World2 &world;
  MovementSystem(World2 &w) : world(w) {}

  void Update(float dt) override;
  void Render(Renderer &renderer) override;
};

class AIMovementSystem : public ISystem {
public:
  World2 &world;
  AIMovementSystem(World2 &w) : world(w) {}

  void Update(float dt) override;
  void Render(Renderer &renderer) override;
};

class AnimationSystem : public ISystem {
public:
  World2 &world;
  AnimationSystem(World2 &w) : world(w) {};
  std::string FacingToString(Direction d) const;
  std::string BuildClipKey(const AnimationState &st);
  void Update(float dt) override;
  void Render(Renderer &) override;
  void OnWorldLoaded(World2 &world);
};

class RenderSystem : public ISystem {
public:
  World2 &world;
  RenderSystem(World2 &w) : world(w) {}
  void Update(float) override;
  void Render(Renderer &renderer) override;
};

} // namespace criogenio
