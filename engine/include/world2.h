#pragma once

#include "components.h"
#include "ecs_core.h"
#include "ecs_registry.h"
#include "raylib.h"
#include "render.h"
#include "systems.h"
#include "terrain.h"
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace criogenio {

// ============================================================================
// World2: New ECS-based World (gradual migration path)
// ============================================================================

class World2 {
public:
  World2();
  ~World2();

  // Entity Management
  ecs::EntityId CreateEntity(const std::string &name = "");
  void DeleteEntity(ecs::EntityId id);
  bool HasEntity(ecs::EntityId id) const;

  // Component Management (new ECS style)
  template <typename T, typename... Args>
  T &AddComponent(ecs::EntityId entity_id, Args &&...args) {
    T component(std::forward<Args>(args)...);
    return ecs::Registry::instance().add_component<T>(entity_id, component);
  }

  template <typename T> T *GetComponent(ecs::EntityId entity_id) {
    return ecs::Registry::instance().get_component<T>(entity_id);
  }

  template <typename T> const T *GetComponent(ecs::EntityId entity_id) const {
    return ecs::Registry::instance().get_component<T>(entity_id);
  }

  template <typename T> bool HasComponent(ecs::EntityId entity_id) const {
    return ecs::Registry::instance().has_component<T>(entity_id);
  }

  template <typename T> void RemoveComponent(ecs::EntityId entity_id) {
    ecs::Registry::instance().remove_component<T>(entity_id);
  }

  // Query (new ECS style - more efficient)
  template <typename T> std::vector<ecs::EntityId> GetEntitiesWith() {
    return ecs::Registry::instance().view<T>();
  }

  template <typename T, typename U>
  std::vector<ecs::EntityId> GetEntitiesWith() {
    return ecs::Registry::instance().view<T, U>();
  }

  template <typename T, typename U, typename V>
  std::vector<ecs::EntityId> GetEntitiesWith() {
    return ecs::Registry::instance().view<T, U, V>();
  }

  template <typename T, typename U, typename V, typename W>
  std::vector<ecs::EntityId> GetEntitiesWith() {
    return ecs::Registry::instance().view<T, U, V, W>();
  }

  // System Management
  template <typename T, typename... Args> T *AddSystem(Args &&...args) {
    auto sys = std::make_unique<T>(std::forward<Args>(args)...);
    T *ptr = sys.get();
    systems.push_back(std::move(sys));
    return ptr;
  }

  // World Update
  void OnUpdate(std::function<void(float)> fn);
  void Update(float dt);
  void Render(Renderer &renderer);

  // Serialization
  SerializedWorld Serialize() const;
  void Deserialize(const SerializedWorld &data);

  // Terrain
  Terrain2D &CreateTerrain2D(const std::string &name,
                             const std::string &texture_path);

  // Camera
  void AttachCamera2D(Camera2D cam);
  Camera2D maincamera;

  // Get all entities
  const std::vector<ecs::EntityId> &GetAllEntities() const {
    return ecs::EntityManager::instance().get_alive_entities();
  }

private:
  std::vector<std::unique_ptr<ISystem>> systems;
  std::unique_ptr<Terrain2D> terrain;
  std::function<void(float)> userUpdate = nullptr;

  // Entity name tracking (optional, for debugging)
  std::unordered_map<ecs::EntityId, std::string> entity_names;
};

} // namespace criogenio
