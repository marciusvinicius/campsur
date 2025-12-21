#pragma once

#include "entity.h"
#include "raylib.h"
#include "render.h"
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "components.h"
#include "systems.h"
#include "terrain.h"

namespace criogenio {

struct Scene {};
struct GameMode {};
struct GameState {};

class World {
public:
  World();
  ~World();
  void OnUpdate(std::function<void(float)> fn);
  void Update(float dt);
  void Render(Renderer &renderer);

public:
  // Templates
  // Move all this to a entity manager system
  //
  template <typename T, typename... Args>
  T *AddComponent(int entityId, Args &&...args) {
    // 1— Construct the component
    auto comp = std::make_unique<T>(std::forward<Args>(args)...);

    // 2— Save raw pointer *before* moving the unique_ptr
    T *ptr = comp.get();

    // 3— Register in type registry
    ComponentTypeId typeId = GetComponentTypeId<T>();
    registry[typeId].push_back(entityId);

    // 4— Attach to entity
    entities[entityId].push_back(std::move(comp));

    // 5— Return pointer for configuration
    return ptr;
  }

  template <typename T, typename... Args>
  void RemoveComponent(int entityId, Args &&...args) {
    // Remove component from the entity and from the registry
    auto it = entities.find(entityId);
    if (it == entities.end())
      throw std::runtime_error("Entity not found");
    auto &comps = it->second;
    for (auto compIt = comps.begin(); compIt != comps.end(); ++compIt) {
      if (auto casted = dynamic_cast<T *>(compIt->get())) {
        // Remove from registry
        ComponentTypeId typeId = GetComponentTypeId<T>();
        auto &entityList = registry[typeId];
        entityList.erase(
            std::remove(entityList.begin(), entityList.end(), entityId),
            entityList.end());
        // Remove from entity
        comps.erase(compIt);
        return;
      }
    }
    throw std::runtime_error("Component not found");
  }

  template <typename T> T &GetComponent(int entityId) {
    auto it = entities.find(entityId);
    if (it == entities.end())
      throw std::runtime_error("Entity not found");

    auto &comps = it->second;
    for (auto &c : comps) {
      if (auto casted = dynamic_cast<T *>(c.get()))
        return *casted;
    }
    throw std::runtime_error("Component not found");
  }

  template <typename T> std::vector<int> GetEntitiesWith() {
    ComponentTypeId typeId = GetComponentTypeId<T>();
    if (registry.contains(typeId))
      return registry[typeId];
    return {};
  }
  template <typename T, typename... Args> T *AddSystem(Args &&...args) {
    auto sys = std::make_unique<T>(std::forward<Args>(args)...);
    T *ptr = sys.get();
    systems.push_back(std::move(sys));
    return ptr;
  }

  template <typename T> bool HasComponent(int entityId) const {
    ComponentTypeId typeId = GetComponentTypeId<T>();

    auto it = registry.find(typeId);
    if (it == registry.end())
      return false;

    const auto &entitiesWithComponent = it->second;
    return std::find(entitiesWithComponent.begin(), entitiesWithComponent.end(),
                     entityId) != entitiesWithComponent.end();
  }

  int CreateEntity(const std::string &name);
  void DeleteEntity(int id);
  bool HasEntity(int id) const;
  Terrain2D &CreateTerrain2D(const std::string &name,
                             const std::string &texture_path);
  void AttachCamera2D(Camera2D cam);

  Camera2D maincamera;

private:
  int nextId = 1;
  std::unordered_map<int, std::vector<std::unique_ptr<Component>>> entities;
  std::vector<std::unique_ptr<ISystem>> systems;
  std::unordered_map<ComponentTypeId, std::vector<int>> registry;
  std::unique_ptr<Terrain2D> terrain;
  std::function<void(float)> userUpdate = nullptr;
};
} // namespace criogenio
