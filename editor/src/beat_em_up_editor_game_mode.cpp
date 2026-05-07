#include "beat_em_up_editor_game_mode.h"
#include "editor.h"
#include "project_context.h"
#include "animated_component.h"
#include "components.h"
#include "core_systems.h"
#include "graphics_types.h"
#include "json_serialization.h"
#include "json.hpp"
#include "world.h"

#include "imgui.h"

#include <cctype>
#include <cstdio>
#include <exception>
#include <string>

using namespace criogenio;

namespace {

/** Slower walk along Y than X — Streets-style lane movement. */
constexpr float kWalkSpeedX = 285.f;
constexpr float kWalkSpeedY = 155.f;

static std::string ToLowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

static ecs::EntityId FindNamedPlayer(World &w) {
  for (ecs::EntityId id : w.GetEntitiesWith<Name, Transform>()) {
    auto *n = w.GetComponent<Name>(id);
    if (n && ToLowerAscii(n->name) == "player")
      return id;
  }
  return ecs::NULL_ENTITY;
}

static ecs::EntityId FindFirstRenderable(World &w) {
  for (ecs::EntityId id : w.GetEntitiesWith<Sprite, Transform>())
    return id;
  for (ecs::EntityId id : w.GetEntitiesWith<AnimatedSprite, Transform>())
    return id;
  return ecs::NULL_ENTITY;
}

static void CameraTargetOnEntity(World &w, ecs::EntityId id) {
  auto *tr = w.GetComponent<Transform>(id);
  if (!tr)
    return;
  float cx = tr->x;
  float cy = tr->y;
  if (auto *sp = w.GetComponent<Sprite>(id)) {
    float sx = (tr->scale_x > 0.f) ? tr->scale_x : 1.f;
    float sy = (tr->scale_y > 0.f) ? tr->scale_y : 1.f;
    cx += 0.5f * static_cast<float>(sp->spriteSize) * sx;
    cy += 0.5f * static_cast<float>(sp->spriteSize) * sy;
  } else if (auto *asp = w.GetComponent<AnimatedSprite>(id)) {
    Rect fr = asp->GetFrame();
    float sx = (tr->scale_x > 0.f) ? tr->scale_x : 1.f;
    float sy = (tr->scale_y > 0.f) ? tr->scale_y : 1.f;
    cx += 0.5f * fr.width * sx;
    cy += 0.5f * fr.height * sy;
  }
  Camera2D *cam = w.GetActiveCamera();
  if (cam) {
    cam->target.x = cx;
    cam->target.y = cy;
  }
}

} // namespace

void BeatEmUpEditorGameMode::Start(EditorApp &app, const ProjectContext *project) {
  (void)project;
  World &world = app.GetWorld();
  world.SetSpriteSortByGroundY(true);
  worldForTick = &world;

  editorCamera = *world.GetActiveCamera();

  try {
    nlohmann::json j = ToJson(world.Serialize("."));
    worldSnapshot = j.dump();
  } catch (const std::exception &e) {
    std::fprintf(stderr, "[BeatEmUpPlay] snapshot failed: %s\n", e.what());
    worldSnapshot.clear();
  }

  playEntity = FindNamedPlayer(world);
  if (playEntity == ecs::NULL_ENTITY)
    playEntity = FindFirstRenderable(world);

  spawnedTransientEntity = false;
  renamedForMovement = false;
  originalEntityName.clear();
  addedController = false;
  addedAnimationState = false;
  addedPlayName = false;

  if (playEntity == ecs::NULL_ENTITY) {
    playEntity = world.CreateEntity("player");
    world.AddComponent<Name>(playEntity, "player");
    world.AddComponent<Transform>(playEntity, 100.f, 100.f);
    spawnedTransientEntity = true;
  } else {
    if (auto *n = world.GetComponent<Name>(playEntity)) {
      if (ToLowerAscii(n->name) != "player") {
        originalEntityName = n->name;
        renamedForMovement = true;
        n->name = "player";
      }
    } else {
      world.AddComponent<Name>(playEntity, "player");
      addedPlayName = true;
    }
  }

  if (!world.HasComponent<Controller>(playEntity)) {
    world.AddComponent<Controller>(playEntity,
                                   Vec2{kWalkSpeedX, kWalkSpeedY});
    addedController = true;
  } else {
    if (auto *c = world.GetComponent<Controller>(playEntity)) {
      c->velocity.x = kWalkSpeedX;
      c->velocity.y = kWalkSpeedY;
    }
  }
  if (!world.HasComponent<AnimationState>(playEntity)) {
    world.AddComponent<AnimationState>(playEntity);
    addedAnimationState = true;
  }

  world.ClearSystems();
  world.AddSystem<MovementSystem>(world);
  world.AddSystem<AnimationSystem>(world);
  world.AddSystem<SpriteSystem>(world);
  world.AddSystem<RenderSystem>(world);

  Camera2D cam = editorCamera;
  if (cam.zoom < 1e-4f)
    cam.zoom = 1.f;
  world.AttachCamera2D(cam);
  CameraTargetOnEntity(world, playEntity);

  std::printf("[BeatEmUpPlay] Started (entity %u). WASD — slower vertical (lane).\n",
              static_cast<unsigned>(playEntity));
}

void BeatEmUpEditorGameMode::Stop(EditorApp &app, bool revert) {
  World &world = app.GetWorld();
  world.SetSpriteSortByGroundY(false);
  worldForTick = nullptr;

  const ecs::EntityId ent = playEntity;

  if (spawnedTransientEntity && ent != ecs::NULL_ENTITY && world.HasEntity(ent))
    world.DeleteEntity(ent);

  world.ClearSystems();
  app.RegisterEditorSystems();

  if (revert && !worldSnapshot.empty()) {
    try {
      nlohmann::json j = nlohmann::json::parse(worldSnapshot);
      SerializedWorld sw = FromJson(j);
      world.Deserialize(sw, ".");
      std::printf("[BeatEmUpPlay] World reverted to pre-play snapshot.\n");
    } catch (const std::exception &e) {
      std::fprintf(stderr, "[BeatEmUpPlay] ERROR reverting snapshot: %s\n", e.what());
    }
  } else if (!spawnedTransientEntity && ent != ecs::NULL_ENTITY && world.HasEntity(ent)) {
    if (addedPlayName)
      world.RemoveComponent<Name>(ent);
    else if (renamedForMovement) {
      if (auto *n = world.GetComponent<Name>(ent))
        n->name = originalEntityName;
    }
    if (addedController)
      world.RemoveComponent<Controller>(ent);
    if (addedAnimationState)
      world.RemoveComponent<AnimationState>(ent);
  }

  *world.GetActiveCamera() = editorCamera;
  playEntity = ecs::NULL_ENTITY;
  spawnedTransientEntity = false;
  renamedForMovement = false;
  addedController = false;
  addedAnimationState = false;
  addedPlayName = false;

  std::printf("[BeatEmUpPlay] Stopped (revert=%s).\n", revert ? "true" : "false");
}

void BeatEmUpEditorGameMode::Tick(float dt) {
  (void)dt;
  if (!worldForTick || playEntity == ecs::NULL_ENTITY ||
      !worldForTick->HasEntity(playEntity))
    return;
  CameraTargetOnEntity(*worldForTick, playEntity);
}

void BeatEmUpEditorGameMode::DrawHUD() {
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoBackground;

  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10.f, vp->WorkPos.y + vp->WorkSize.y - 88.f));
  ImGui::SetNextWindowSize(ImVec2(420.f, 78.f));
  ImGui::SetNextWindowBgAlpha(0.55f);

  if (ImGui::Begin("##beatemup_play_hud", nullptr, flags & ~ImGuiWindowFlags_NoBackground)) {
    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.55f, 1.f), "Beat 'em up play");
    ImGui::TextDisabled(
        "WASD / arrows  ·  Shift run  ·  depth by Y  ·  Stop to exit");
  }
  ImGui::End();
}
