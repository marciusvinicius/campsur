#pragma once
#include "components.h"
#include "event.h"
#include "scene.h"
#include <set>
#include <utility>

namespace criogenio {

class CollisionSystem {
public:
  CollisionSystem(Scene &scene, EventBus &bus);

  void Update();

private:
  Scene &scene;
  EventBus &bus;

  // Track existing collisions
  std::set<std::pair<int, int>> activeCollisions;
};

inline bool AABB(const Transform &a, const Collider &ca, const Transform &b,
                 const Collider &cb) {
  return a.x < b.x + cb.width && a.x + ca.width > b.x &&
         a.y < b.y + cb.height && a.y + ca.height > b.y;
};

class CameraSystem {
public:
  Camera2D *GetActiveCamera(Scene &scene);
  static void Update(Scene &scene, float dt);
};

} // namespace criogenio
