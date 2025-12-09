#include "scene.h"

#include "engine.h"
#include "raylib.h"
#include "raymath.h"
#include <charconv>

namespace criogenio {

Scene::Scene() {
  maincamera = Camera2D{};
  maincamera.target = {0.0f, 0.0f};
  maincamera.offset = {800 / 2.0f, 600 / 2.0f}; // IMPORTANT
  maincamera.zoom = 1.0f;
}

Scene::~Scene() {
  for (auto &e : entities) {
    if (e->sprite.loaded) {
      UnloadTexture(e->sprite.texture);
    }
  }
}

Entity &Scene::CreateEntity(const std::string &name) {
  auto ent = std::make_unique<Entity>(nextId++, name);
  Entity &ref = *ent;
  entities.push_back(std::move(ent));
  return ref;
}

Terrain &Scene::CreateTerrain(const std::string &name,
                              const std::string &texture_path) {
  auto new_terrain = new Terrain();
  auto atlas = CriogenioLoadTexture(texture_path.c_str());
  auto tileset = Tileset{atlas, 32, 10, 8};
  new_terrain->tileset = tileset;
  new_terrain->layers.push_back(
      TileLayer(10, 10,
                {
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

                }));
  terrain.reset(new_terrain);
  // terrain = &new_terrain;
  return *terrain;
}

void Scene::Update(float dt) {
  if (userUpdate)
    userUpdate(dt);
}

void Scene::Render(Renderer &renderer) {
  // Render on world position for now
  if (terrain)
    terrain->Render(renderer);
  // TODO:(maraujo) Entity dont have a animation or sprite yet
  for (auto &e : entities) {
    if (e->sprite.loaded) {
      // TODO: Not broking when load error
      renderer.DrawTexture(e->sprite.texture, e->transform.x, e->transform.y);
      renderer.DrawRect(e->transform.x, e->transform.y, 32, 32, RED);
    } else {
      renderer.DrawRect(e->transform.x, e->transform.y, 32, 32, RED);
    }
  }
}

Entity &Scene::GetEntityById(int id) {
  for (auto &e : entities) {
    if (e->id == id)
      return *e;
  }
  throw std::runtime_error("Entity not found");
}
std::vector<std::unique_ptr<Entity>> &Scene::GetEntities() { return entities; }

void Scene::DeleteEntity(int id) {
  entities.erase(std::remove_if(entities.begin(), entities.end(),
                                [&](auto &e) { return e->id == id; }),
                 entities.end());
}

void Scene::OnUpdate(std::function<void(float)> fn) { userUpdate = fn; }

bool Scene::HasEntity(int id) const {
  for (const auto &e : entities) {
    if (e->id == id)
      return true;
  }
  return false;
}

} // namespace criogenio
