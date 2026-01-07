#include "animation_database.h"
#include "components.h"
#include "criogenio_io.h"
#include "engine.h"
#include "game.h"

// TODO:(maraujo) remove this and create this on engine
#include "raylib.h"

using namespace criogenio;

int main() {
  Engine engine(InitialWidth, InitialHeight, "Ways of the Truth");

  auto &World = engine.GetWorld();

  // Load the scene from editor
  if (!LoadWorldFromFile(World, "world.json")) {
    TraceLog(LOG_ERROR, "Failed to load world from world.json");
    return 1;
  }

  engine.RegisterCoreComponents();
  // Add game systems
  World.AddSystem<criogenio::MovementSystem>(World);
  World.AddSystem<criogenio::AnimationSystem>(World);
  World.AddSystem<criogenio::RenderSystem>(World);
  World.AddSystem<criogenio::AIMovementSystem>(World);

  // Setup camera
  auto maincamera = Camera2D{};
  maincamera.target = {0.0f, 0.0f};
  maincamera.offset = {InitialWidth / 2.0f, InitialHeight / 2.0f};
  maincamera.zoom = 1.0f;
  World.AttachCamera2D(maincamera);

  // Add custom game system
  static_assert(std::is_base_of_v<criogenio::ISystem, MySystem>);
  static_assert(!std::is_abstract_v<MySystem>);
  World.AddSystem<MySystem>(World);

  engine.Run();
  return 0;
}
