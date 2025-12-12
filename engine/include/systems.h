#pragma once
#include "animation_state.h"
#include "components.h"
#include "render.h"

namespace criogenio {
class Scene;

class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void Update(float dt) = 0;
  virtual void Render(Renderer &renderer) = 0;
};

} // namespace criogenio
