#pragma once

#include "components.h"
#include "scene.h"
#include "systems.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
namespace criogenio {
class MovementSystem : public ISystem {
public:
  Scene &scene;
  MovementSystem(Scene &s) : scene(s) {}

  void Update(float dt) override;
  void Render(Renderer&) override;
};

class AnimationSystem : public ISystem {
public:
  Scene &scene;
  AnimationSystem(Scene &s) : scene(s) {};
  std::string FacingToString(Direction d) const;
  std::string BuildClipKey(const AnimationState& st);
  void Update(float dt) override;
  void Render(Renderer&) override;
};

class RenderSystem : public ISystem {
public:
  Scene &scene;
  RenderSystem(Scene &s) : scene(s) {}
  void Update(float) override;
  void Render(Renderer& renderer) override;
};

} // namespace criogenio