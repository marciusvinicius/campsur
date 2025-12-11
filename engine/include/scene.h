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
#include "terrain.h"

namespace criogenio {

class Scene {
public:
  Scene();
  ~Scene();

  void OnUpdate(std::function<void(float)> fn);
  void Update(float dt);
  void Render(Renderer &renderer);

public:
  // Templates
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

  // std::vector<int> &GetEntities();
  // const std::vector<std::unique_ptr<Entity>> &GetEntities() const;
  int CreateEntity(const std::string &name);
  void DeleteEntity(int id);
  bool HasEntity(int id) const;
  Terrain2D &CreateTerrain2D(const std::string &name,
                             const std::string &texture_path);
  void AttachCamera2D(Camera2D cam);

  // TODO:(maraujo) move this to private
  Camera2D maincamera;

private:
  int nextId = 1;
  std::unordered_map<int, std::vector<std::unique_ptr<Component>>> entities;

  std::unordered_map<ComponentTypeId, std::vector<int>>
      registry; // <--- MISSING
  std::unique_ptr<Terrain2D> terrain;
  std::function<void(float)> userUpdate = nullptr;
};
} // namespace criogenio
