#pragma once

#include "ecs_core.h"
#include "graphics_types.h"
#include "i_editor_game_mode.h"
#include <string>

namespace criogenio {
class World;
}

/**
 * Minimal in-editor play mode for non-Subterra 2D projects (tilemap optional).
 * Uses the core MovementSystem (WASD / arrows, Shift to run) on one
 * "possessed" entity: prefer `Name` case-insensitive "player", else the first
 * entity with `Transform` + `Sprite` or `AnimatedSprite`. Temporarily renames
 * that entity's `Name` to "player" when needed so `MovementSystem` drives it.
 */
class FreeForm2DEditorGameMode : public IEditorGameMode {
public:
  std::string worldSnapshot;
  criogenio::Camera2D editorCamera{};
  /** Valid between Start and Stop; used by Tick for camera follow. */
  criogenio::World *worldForTick = nullptr;
  criogenio::ecs::EntityId playEntity = criogenio::ecs::NULL_ENTITY;
  bool spawnedTransientEntity = false;
  bool renamedForMovement = false;
  std::string originalEntityName;
  bool addedController = false;
  bool addedAnimationState = false;
  /** We added `Name{"player"}` because the possessed entity had no Name. */
  bool addedPlayName = false;

  void Start(EditorApp &app, const ProjectContext *project) override;
  void Stop(EditorApp &app, bool revert) override;
  void Tick(float dt) override;
  void DrawHUD() override;
};
