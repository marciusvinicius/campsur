#include "game.h"
#include "terrain_loader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

using namespace criogenio;
using namespace subterra;

namespace {

const char *kDefaultMapPath = "assets/levels/Cave.tmx";

void printUsage(const char *argv0) {
  std::fprintf(stderr,
               "Usage: %s [--map <path.tmx>] [--help]\n"
               "  Default map: %s (run from subterra_guild/)\n",
               argv0, kDefaultMapPath);
}

}  // namespace

int main(int argc, char **argv) {
  GameEngine engine(ScreenWidth, ScreenHeight, "Subterra Guild");
  std::string mapPath = kDefaultMapPath;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
      mapPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      printUsage(argv[0]);
      return 0;
    }
  }

  World &world = engine.GetWorld();
  engine.RegisterCoreComponents();

  try {
    Terrain2D terrain = TilemapLoader::LoadFromTMX(mapPath);
    std::printf("Loaded TMX: %s (%d×%d tiles, %d×%d px cell)\n", mapPath.c_str(),
                 terrain.LogicalMapWidthTiles(), terrain.LogicalMapHeightTiles(),
                 terrain.GridStepX(), terrain.GridStepY());
    world.SetTerrain(std::make_unique<Terrain2D>(std::move(terrain)));
  } catch (const std::exception &e) {
    std::fprintf(stderr, "Failed to load map \"%s\": %s\n", mapPath.c_str(), e.what());
    return 1;
  }

  world.AddSystem<VelocityMovementSystem>(world);
  world.AddSystem<CameraFollowSystem>(world);
  world.AddSystem<PlayerRenderSystem>(world);

  Terrain2D *ter = world.GetTerrain();
  float spawnX = 0.f;
  float spawnY = 0.f;
  float camX = static_cast<float>(MapWidthTiles * TileSize) * 0.5f;
  float camY = static_cast<float>(MapHeightTiles * TileSize) * 0.5f;
  if (ter && ter->LogicalMapWidthTiles() > 0) {
    spawnX = (ter->LogicalMapWidthTiles() * 0.5f - 0.5f) * static_cast<float>(ter->GridStepX());
    spawnY = (ter->LogicalMapHeightTiles() * 0.5f - 0.5f) * static_cast<float>(ter->GridStepY());
    camX = ter->LogicalMapWidthTiles() * static_cast<float>(ter->GridStepX()) * 0.5f;
    camY = ter->LogicalMapHeightTiles() * static_cast<float>(ter->GridStepY()) * 0.5f;
  }

  criogenio::ecs::EntityId player = world.CreateEntity("player");
  world.AddComponent<Name>(player, "player");
  world.AddComponent<Transform>(player, spawnX, spawnY);
  world.AddComponent<Controller>(player, Vec2{0.f, 0.f});

  Camera2D cam = {};
  cam.offset = {0.f, 0.f};
  cam.target = {camX, camY};
  cam.zoom = 1.f;
  world.AttachCamera2D(cam);

  engine.Run();
  return 0;
}
