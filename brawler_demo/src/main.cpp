#include "animated_component.h"
#include "animation_database.h"
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

/** Visual center in world space (`Sprite` / `AnimatedSprite` use top-left `Transform`). */
bool PlayerVisualCenter(World &w, ecs::EntityId id, float *outW, float *outH, Vec2 *outCenter) {
  auto *tr = w.GetComponent<Transform>(id);
  if (!tr || !outCenter)
    return false;
  if (auto *sp = w.GetComponent<Sprite>(id)) {
    const float sx = tr->scale_x > 0.f ? tr->scale_x : 1.f;
    const float sy = tr->scale_y > 0.f ? tr->scale_y : 1.f;
    const float ww = static_cast<float>(sp->spriteSize) * sx;
    const float hh = static_cast<float>(sp->spriteSize) * sy;
    if (outW)
      *outW = ww;
    if (outH)
      *outH = hh;
    outCenter->x = tr->x + ww * 0.5f;
    outCenter->y = tr->y + hh * 0.5f;
    return true;
  }
  if (auto *asp = w.GetComponent<AnimatedSprite>(id)) {
    const auto *def = AnimationDatabase::instance().getAnimation(asp->animationId);
    if (!def)
      return false;
    Rect fr = asp->GetFrame();
    const float sx = tr->scale_x > 0.f ? tr->scale_x : 1.f;
    const float sy = tr->scale_y > 0.f ? tr->scale_y : 1.f;
    const float ww = fr.width * sx;
    const float hh = fr.height * sy;
    if (outW)
      *outW = ww;
    if (outH)
      *outH = hh;
    outCenter->x = tr->x + ww * 0.5f;
    outCenter->y = tr->y + hh * 0.5f;
    return true;
  }
  if (outW)
    *outW = 32.f;
  if (outH)
    *outH = 32.f;
  outCenter->x = tr->x;
  outCenter->y = tr->y;
  return true;
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
    float fw = 32.f, fh = 32.f;
    Vec2 center{};
    if (!PlayerVisualCenter(w, playerId, &fw, &fh, &center))
      return;
    CameraFollow2DConfig cfg;
    cfg.smoothingSpeed = 0.f;
    cfg.deadzoneHalfWidth = 0.f;
    cfg.deadzoneHalfHeight = 0.f;
    (void)UpdateCameraFollow2D(w, cam, playerId, fw, fh, dt, cfg);
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
  cam.offset = {0.f, 0.f};
  cam.zoom = 1.f;
  if (engine.playerId != ecs::NULL_ENTITY) {
    Vec2 pc{};
    float pw = 32.f, ph = 32.f;
    if (PlayerVisualCenter(w, engine.playerId, &pw, &ph, &pc)) {
      cam.target.x = pc.x;
      cam.target.y = pc.y;
    }
  }
  w.AttachCamera2D(cam);

  engine.Run();
  return 0;
}
