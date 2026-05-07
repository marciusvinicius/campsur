#include "animated_component.h"
#include "components.h"
#include "core_systems.h"
#include "criogenio_io.h"
#include "engine.h"
#include "world.h"

#include <cctype>
#include <cstdio>
#include <filesystem>

using namespace criogenio;

namespace fs = std::filesystem;

namespace {

ecs::EntityId FindPlayer(World &w) {
  for (auto id : w.GetEntitiesWith<Name>()) {
    auto *n = w.GetComponent<Name>(id);
    if (!n)
      continue;
    std::string s = n->name;
    for (char &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s == "player")
      return id;
  }
  return ecs::NULL_ENTITY;
}

} // namespace

class BrawlerEngine : public Engine {
public:
  ecs::EntityId playerId = ecs::NULL_ENTITY;
  BrawlerEngine() : Engine(1280, 720, "Brawler demo") {}

protected:
  void OnFrame(float dt) override {
    World &w = GetWorld();
    Camera2D *cam = w.GetActiveCamera();
    if (!cam || playerId == ecs::NULL_ENTITY || !w.HasEntity(playerId))
      return;
    CameraFollow2DConfig cfg;
    cfg.smoothingSpeed = 8.f;
    cfg.deadzoneHalfWidth = 96.f;
    cfg.deadzoneHalfHeight = 56.f;
    UpdateCameraFollow2D(w, cam, playerId, 32.f, 32.f, dt, cfg);
  }
};

int main(int argc, char **argv) {
  const fs::path demoRoot = fs::path(__FILE__).parent_path().parent_path();
  std::error_code ec;
  fs::current_path(demoRoot, ec);

  BrawlerEngine engine;
  engine.RegisterCoreComponents();
  World &w = engine.GetWorld();

  fs::path levelPath = demoRoot / "assets/levels/stage01.campsurlevel";
  if (argc > 1)
    levelPath = fs::path(argv[1]);

  if (!LoadWorldFromFile(w, levelPath.string())) {
    std::fprintf(stderr, "[Brawler] Failed to load '%s' (cwd should be brawler_demo/)\n",
                 levelPath.string().c_str());
    return 1;
  }

  w.SetSpriteSortByGroundY(true);
  engine.playerId = FindPlayer(w);

  w.ClearSystems();
  w.AddSystem<MovementSystem>(w);
  w.AddSystem<AnimationSystem>(w);
  w.AddSystem<SpriteSystem>(w);
  w.AddSystem<RenderSystem>(w);

  Camera2D cam{};
  cam.target = {440.f, 280.f};
  cam.offset = {0.f, 0.f};
  cam.zoom = 1.f;
  w.AttachCamera2D(cam);

  engine.Run();
  return 0;
}
