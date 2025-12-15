#pragma once

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
};

class RenderSystem : public ISystem {
public:
  World &world;
  RenderSystem(World &w) : world(w) {}
  void Update(float) override;
  void Render(Renderer &renderer) override;
};

} // namespace criogenio