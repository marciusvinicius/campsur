#include "world.h"
#include "components.h"
#include "core.h"
#include "engine.h"
#include "raylib.h"
#include "raymath.h"

namespace criogenio {

World::World() {}

World::~World() {
  // clear all entities and components
  entities.clear();
  if (terrain) {
    UnloadTexture(terrain->tileset.atlas);
    terrain.reset();
  }
}

int World::CreateEntity(const std::string &name) {
  int id = nextId++;
  entities[id] = std::vector<std::unique_ptr<Component>>();
  AddComponent<criogenio::Name>(id, "New Entity");
  return id;
}

Terrain2D &World::CreateTerrain2D(const std::string &name,
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

void World::Update(float dt) {
  if (userUpdate)
    userUpdate(dt);
  for (auto &sys : systems)
    sys->Update(dt);
}

void World::Render(Renderer &renderer) {

  BeginMode2D(maincamera);
  DrawGrid(100, 32);
  DrawCircle(0, 0, 6, RED);
  if (terrain)
    terrain->Render(renderer);

  for (auto &sys : systems)
    sys->Render(renderer);

  EndMode2D();
}

// Should I remove the map values?
void World::DeleteEntity(int id) { entities.erase(id); }

void World::OnUpdate(std::function<void(float)> fn) { userUpdate = fn; }

bool World::HasEntity(int id) const {
  return entities.find(id) != entities.end();
}

void World::AttachCamera2D(Camera2D cam) {
  // For now we have only one camera
  maincamera = cam;
}

} // namespace criogenio
