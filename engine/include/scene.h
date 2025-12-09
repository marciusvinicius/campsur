#pragma once
#include "components.h"
#include "entity.h"
#include "raylib.h"
#include "renderer.h"
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#include "terrain.h"

namespace criogenio {
class Scene {
public:
  Scene();
  ~Scene();

  Entity &CreateEntity(const std::string &name);
  Entity &GetEntityById(int id);
  std::vector<std::unique_ptr<Entity>> &GetEntities();
  const std::vector<std::unique_ptr<Entity>> &GetEntities() const;

  void DeleteEntity(int id);
  void Update(float dt);
  void Render(Renderer &renderer);
  bool HasEntity(int id) const;

  Terrain &CreateTerrain(const std::string &name,
                         const std::string &atlas_path);
  void OnUpdate(std::function<void(float)> fn);

  Camera2D maincamera;

private:
  int nextId = 1;
  std::vector<std::unique_ptr<Entity>> entities;
  std::unique_ptr<Terrain> terrain;
  std::function<void(float)> userUpdate = nullptr;
};
} // namespace criogenio
