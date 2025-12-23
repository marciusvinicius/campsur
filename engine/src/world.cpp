#include "world.h"
#include "component_factory.h"
#include "components.h"
#include "core.h"
#include "engine.h"
#include "raylib.h"
#include "raymath.h"
#include <cassert>

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

const std::unordered_map<int, std::vector<std::unique_ptr<Component>>> &
World::GetEntities() const {
  return entities;
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

SerializedWorld World::Serialize() const {
  SerializedWorld worldData;

  for (const auto &[entityId, components] : entities) {
    SerializedEntity ent;
    ent.id = entityId;

    for (const auto &comp : components) {
      ent.components.push_back(comp->Serialize());
    }

    worldData.entities.push_back(std::move(ent));
  }

  return worldData;
}

void World::Deserialize(const SerializedWorld &data) {
  // Clear existing world
  entities.clear();
  registry.clear();
  systems.clear();
  terrain.reset();
  nextId = 1;

  for (const SerializedEntity &ent : data.entities) {
    int entityId = ent.id;

    entities[entityId] = std::vector<std::unique_ptr<Component>>();

    // Ensure entity container exists

    // Keep nextId valid
    nextId = std::max(nextId, entityId + 1);

    for (const SerializedComponent &comp : ent.components) {

      if (comp.type == "Name") {
        auto name = GetString(comp.fields.at("name"));
        AddComponent<Name>(entityId, name);

      }

      else if (comp.type == "Transform") {
        auto *transform = AddComponent<Transform>(entityId);
        transform->Deserialize(comp);
      }

      else if (comp.type == "AnimatedSprite") {
        auto *sprite = AddComponent<AnimatedSprite>(entityId);
        sprite->Deserialize(comp);
      }

      else if (comp.type == "AnimationState") {
        auto *state = AddComponent<AnimationState>(entityId);
        state->Deserialize(comp);
      }

      else {
        std::cerr << "Unknown component type: " << comp.type << "\n";
      }
    }
  }
}
} // namespace criogenio
