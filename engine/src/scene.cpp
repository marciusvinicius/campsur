#include "scene.h"
#include "components.h"
#include "engine.h"
#include "raylib.h"
#include "raymath.h"

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
  AddComponent<criogenio::Name>(id, "New Entity");
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

  // Update animated sprites
  auto entityIdsAnimatedSprite = GetEntitiesWith<AnimatedSprite>();
  if (!entityIdsAnimatedSprite.empty()) {
    for (auto entityId : entityIdsAnimatedSprite) {
      auto &animSprite = GetComponent<AnimatedSprite>(entityId);
      animSprite.Update(dt);
    }
  }
}

void Scene::Render(Renderer &renderer) {
  BeginMode2D(maincamera);
  DrawGrid(100, 32);
  DrawCircle(0, 0, 6, RED);

  // Draw all sprite components or animated components
  auto entityIdsSprites = GetEntitiesWith<Sprite>();
  if (!entityIdsSprites.empty()) {
    for (auto entityId : entityIdsSprites) {
      auto &sprite = GetComponent<Sprite>(entityId);
      auto &transform = GetComponent<Transform>(entityId);
      Rectangle sourceRec = {32.0f, 32.0f,
                             static_cast<float>(sprite.texture.width),
                             static_cast<float>(sprite.texture.height)};
      Vector2 position = {transform.x, transform.y};
      DrawTextureRec(sprite.texture, sourceRec, position, WHITE);
    }
  }
  // Try to draw animated sprites
  auto entityIdsAnimatedSprite = GetEntitiesWith<AnimatedSprite>();
  if (!entityIdsAnimatedSprite.empty()) {
    for (auto entityId : entityIdsAnimatedSprite) {
      auto &animSprite = GetComponent<AnimatedSprite>(entityId);
      auto &transform = GetComponent<Transform>(entityId);
      Rectangle src = animSprite.GetFrame();
      Rectangle dest = {transform.x, transform.y, static_cast<float>(src.width),
                        static_cast<float>(src.height)};
      DrawTexturePro(animSprite.texture, src, dest, {0, 0}, 0.0f, WHITE);
    }
  }
  if (terrain)
    terrain->Render(renderer);
  EndMode2D();
}

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

} // namespace criogenio
