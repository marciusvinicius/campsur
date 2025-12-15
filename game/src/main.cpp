#include "components.h"
#include "engine.h"
#include "game.h"
#include "world.h"

// TODO:(maraujo) remove this and create this on engine
#include "raylib.h"

using namespace criogenio;

int game_main() {
  Engine engine(InitialWidth, InitialHeight, "Ways of the Truth");

  auto &World = engine.GetWorld();
  World.CreateTerrain2D("MainTerrain", "game/assets/terrain.jpg");

  auto maincamera = Camera2D{};
  maincamera.target = {0.0f, 0.0f};
  maincamera.offset = {InitialWidth / 2.0f, InitialHeight / 2.0f}; // IMPORTANT
  maincamera.zoom = 1.0f;

  World.AttachCamera2D(maincamera);

  int entityId = World.CreateEntity("Player");
  World.AddComponent<criogenio::Transform>(entityId);

  engine.Run();
  return 0;
}
