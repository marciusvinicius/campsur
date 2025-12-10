#include "scene.h"

#include "engine.h"
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <charconv>
#include <memory>

namespace criogenio {

Scene::Scene() {}

Scene::~Scene() {
  // clear all entities and components
  entities.clear();
  if (terrain) {
    UnloadTexture(terrain->tileset.atlas);
    terrain.reset();
  }
}

int Scene::CreateEntity(const std::string &name) {
  int id = nextId++;
  entities[id] = std::vector<std::unique_ptr<Component>>();
  AddComponent<Name>(id, name);
  return id;
}

Terrain2D &Scene::CreateTerrain2D(const std::string &name,
                                  const std::string &texture_path) {
  auto new_terrain = new Terrain2D();
  auto atlas = CriogenioLoadTexture(texture_path.c_str());
  // This should be created on editor or loaded from file
  auto tileset = Tileset{atlas, 24, 10, 8};
  new_terrain->tileset = tileset;
  // Layers should be loaded from a file, but for now we create a simple one
  // Or Empty
  new_terrain->layers.push_back(
      TileLayer(10, 10,
                {
                    0, 0, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                }));
  terrain.reset(new_terrain);
  // terrain = &new_terrain;
  return *terrain;
}

void Scene::Update(float dt) {
  if (userUpdate)
    userUpdate(dt);

  // update all components
  for (auto &[entityId, components] : entities) {
    for (auto &component : components) {
      // Here you would typically have some kind of update method in your
      // Component class component->Update(dt);
    }
  }
}

void Scene::Render(Renderer &renderer) {
  // Render on world position for now
  BeginMode2D(maincamera);
  DrawGrid(100, 32);
  DrawCircle(0, 0, 6, RED);
  if (terrain)
    terrain->Render(renderer);

  EndMode2D();
}

/**std::vector<int> &Scene::GetEntities() {
  std::vector<int> result;
  result.reserve(entities.size()); // optional but good for performance
  for (const auto &[id, components] : entities) {
    result.push_back(id);
  }
  return result;
}**/

// Should I remove the map values?
void Scene::DeleteEntity(int id) { entities.erase(id); }

void Scene::OnUpdate(std::function<void(float)> fn) { userUpdate = fn; }

bool Scene::HasEntity(int id) const {
  return entities.find(id) != entities.end();
}

void Scene::AttachCamera2D(Camera2D cam) {
  // For now we have only one camera
  maincamera = cam;
}

void Scene::AttachComponentToEntity(int entityId, Component *component) {
  auto &comps = entities[entityId];
  comps.push_back(std::unique_ptr<Component>(component));

  // Register in the type registry
  ComponentTypeId typeId =
      GetComponentTypeId<std::remove_pointer_t<decltype(*component)>>();

  registry[typeId].push_back(entityId);
}

} // namespace criogenio
