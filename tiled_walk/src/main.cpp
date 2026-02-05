#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace criogenio;
using namespace tiled;

int main(int argc, char **argv) {
  GameEngine engine(ScreenWidth, ScreenHeight, "Tiled Walk - Multiplayer");
  uint16_t port = DefaultPort;
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

  World &world = engine.GetWorld();
  engine.RegisterCoreComponents();

  world.AddSystem<TileMapSystem>(world);
  world.AddSystem<VelocityMovementSystem>(world);
  world.AddSystem<PlayerRenderSystem>(world);

  Camera2D cam = {};
  cam.offset = {ScreenWidth / 2.f, ScreenHeight / 2.f};
  cam.target = {MapWidthTiles * TileSize / 2.f, MapHeightTiles * TileSize / 2.f};
  cam.zoom = 1.f;
  world.AttachCamera2D(cam);

  engine.Run();
  return 0;
}
