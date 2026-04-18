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
  world.AddSystem<CameraFollowSystem>(engine, world);
  world.AddSystem<PlayerRenderSystem>(world);

  // Single-player: no replication runs, so create a local player (same net id 0 as server host).
  if (engine.GetNetworkMode() == NetworkMode::Off) {
    criogenio::ecs::EntityId player = world.CreateEntity("player");
    world.AddComponent<NetReplicated>(player);
    world.AddComponent<Transform>(player, 0.f, 0.f);
    world.AddComponent<Controller>(player, Vec2{0.f, 0.f});
    world.AddComponent<ReplicatedNetId>(player, ReplicatedNetId(0));
  }

  Camera2D cam = {};
  // With this renderer, offset 0 keeps cam.target at the viewport center.
  cam.offset = {0.f, 0.f};
  cam.target = {MapWidthTiles * TileSize / 2.f, MapHeightTiles * TileSize / 2.f};
  cam.zoom = 1.f;
  world.AttachCamera2D(cam);

  engine.Run();
  return 0;
}
