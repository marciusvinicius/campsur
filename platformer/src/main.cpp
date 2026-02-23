#include "animation_database.h"
#include "criogenio_io.h"
#include "platformer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace criogenio;

int main(int argc, char** argv) {
  PlatformerEngine engine(PlatformerWidth, PlatformerHeight, "Platformer");
  uint16_t port = PlatformerNetPort;
  const char* clientHost = nullptr;

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

  auto& World = engine.GetWorld();

  if (!LoadWorldFromFile(World, "platformer/level.json")) {
    std::fprintf(stderr, "Failed to load platformer/level.json\n");
    return 1;
  }

  engine.RegisterCoreComponents();
  //auto& gravity = World.AddGlobalComponent<Gravity>();
  //gravity.strength = 980.0f;

  AssetId playerAnimId = AnimationDatabase::instance().createAnimation(
      "assets/brackeys_platformer_assets/sprites/knight.png");
  AnimationDatabase::instance().addClip(playerAnimId,
      {"idle_right", {{0, 0, 32, 32}, {32, 0, 32, 32}}, 0.15f, AnimState::IDLE, Direction::RIGHT});
  AnimationDatabase::instance().addClip(playerAnimId,
      {"idle_left", {{64, 0, 32, 32}, {96, 0, 32, 32}}, 0.15f, AnimState::IDLE, Direction::LEFT});
  AnimationDatabase::instance().addClip(playerAnimId,
      {"walk_right", {{0, 32, 32, 32}, {32, 32, 32, 32}, {64, 32, 32, 32}}, 0.1f, AnimState::WALKING, Direction::RIGHT});
  AnimationDatabase::instance().addClip(playerAnimId,
      {"walk_left", {{0, 64, 32, 32}, {32, 64, 32, 32}, {64, 64, 32, 32}}, 0.1f, AnimState::WALKING, Direction::LEFT});
  AnimationDatabase::instance().addClip(playerAnimId,
      {"jump_right", {{0, 96, 32, 32}}, 0.1f, AnimState::JUMPING, Direction::RIGHT});
  AnimationDatabase::instance().addClip(playerAnimId,
      {"jump_left", {{32, 96, 32, 32}}, 0.1f, AnimState::JUMPING, Direction::LEFT});

  World.AddSystem<PlatformerInitSystem>(World, playerAnimId);
  World.AddSystem<PlatformerMovementSystem>(World);
  //World.AddSystem<GravitySystem>(World);
  World.AddSystem<CollisionSystem>(World);
  World.AddSystem<CameraFollowSystem>(World);
  World.AddSystem<AnimationSystem>(World);
  World.AddSystem<RenderSystem>(World);

  Camera2D maincamera = {};
  maincamera.target = {0.0f, -200.0f};  // Start at player spawn
  maincamera.offset = {0.0f, 0.0f};     // (0,0) centers target on screen
  maincamera.zoom = 1.0f;
  World.AttachCamera2D(maincamera);

  engine.Run();
  return 0;
}
