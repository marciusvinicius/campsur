#include "engine.h"
#include "game.h"
#include "components.h"
#include "scene.h"

// TODO:(maraujo) remove this and create this on engine
#include "raylib.h"

using namespace criogenio;

int main() {
  Engine engine(InitialWidth, InitialHeight, "Ways of the Truth");

  auto &scene = engine.GetScene();
  scene.CreateTerrain2D("MainTerrain", "game/assets/terrain.jpg");

  auto maincamera = Camera2D{};
  maincamera.target = {0.0f, 0.0f};
  maincamera.offset = {InitialWidth / 2.0f, InitialHeight / 2.0f}; // IMPORTANT
  maincamera.zoom = 1.0f;

  scene.AttachCamera2D(maincamera);

  int entityId = scene.CreateEntity("Player");

  criogenio::Transform* transform = scene.AddComponent<criogenio::Transform>(entityId);

  transform->x = 200;
  transform->y = 100;

  engine.Run();
  return 0;
}
