#include "animation_database.h"
#include "components.h"
#include "criogenio_io.h"
#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace criogenio;

int main(int argc, char **argv) {
  GameEngine engine(InitialWidth, InitialHeight, "Ways of the Truth");
  uint16_t port = DefaultNetPort;
  const char *clientHost = nullptr;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--server") == 0) {
      if (i + 1 < argc && argv[i + 1][0] != '-')
        port = static_cast<uint16_t>(std::atoi(argv[++i]));
      if (!engine.StartServer(port)) {
        std::fprintf(stderr, "Failed to start server on port %u\n", port);
        return 1;
      }
      std::printf("Server listening on port %u\n", port);
      break;
    }
    if (std::strcmp(argv[i], "--client") == 0 && i + 1 < argc) {
      clientHost = argv[++i];
      if (i + 1 < argc && argv[i + 1][0] != '-')
        port = static_cast<uint16_t>(std::atoi(argv[++i]));
      if (!engine.ConnectToServer(clientHost, port)) {
        std::fprintf(stderr, "Failed to connect to %s:%u\n", clientHost, port);
        return 1;
      }
      std::printf("Connected to %s:%u\n", clientHost, port);
      break;
    }
  }

  auto &World = engine.GetWorld();

  if (!LoadWorldFromFile(World, "world.json")) {
    std::fprintf(stderr, "Failed to load world from world.json\n");
    return 1;
  }

  engine.RegisterCoreComponents();
  World.AddSystem<MovementSystem>(World);
  World.AddSystem<AnimationSystem>(World);
  World.AddSystem<RenderSystem>(World);
  World.AddSystem<AIMovementSystem>(World);

  Camera2D maincamera = {};
  maincamera.target = {0.0f, 0.0f};
  maincamera.offset = {InitialWidth / 2.0f, InitialHeight / 2.0f};
  maincamera.zoom = 1.0f;
  World.AttachCamera2D(maincamera);

  static_assert(std::is_base_of_v<ISystem, MySystem>);
  static_assert(!std::is_abstract_v<MySystem>);
  World.AddSystem<MySystem>(World);

  engine.Run();
  return 0;
}
