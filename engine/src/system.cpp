#include "system.h"
#include "components.h"
#include "scene.h"

namespace criogenio {

CollisionSystem::CollisionSystem(Scene &scene, EventBus &bus)
    : scene(scene), bus(bus) {}

void CollisionSystem::Update() {
  auto &entities = scene.GetEntities(); // You must add this getter

  std::set<std::pair<int, int>> newCollisions;

  // Check all entity pairs
  for (int i = 0; i < entities.size(); i++) {
    auto &A = *entities[i];
    if (!A.hasCollider)
      continue;

    for (int j = i + 1; j < entities.size(); j++) {
      auto &B = *entities[j];
      if (!B.hasCollider)
        continue;

      bool hit = AABB(A.transform, A.collider, B.transform, B.collider);

      if (hit) {
        auto pair = std::minmax(A.id, B.id);
        newCollisions.insert(pair);

        // Enter event
        if (!activeCollisions.count(pair)) {
          bus.Emit(CollisionEvent(EventType::CollisionEnter, pair.first,
                                  pair.second));
        }
      }
    }
  }

  // Check for exits
  for (auto &col : activeCollisions) {
    if (!newCollisions.count(col)) {
      bus.Emit(CollisionEvent(EventType::CollisionExit, col.first, col.second));
    }
  }

  activeCollisions = newCollisions;
}

} // namespace criogenio
