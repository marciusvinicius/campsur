#pragma once

#include "ecs_core.h"
#include "graphics_types.h"
#include "i_editor_game_mode.h"
#include <string>

namespace criogenio {
class World;
}

/**
 * In-editor play for belt-scroller / beat-'em-up style projects (`game_mode`: "beat_em_up").
 * Like free-form 2D but enables ground-Y sprite sorting and slightly slower vertical
 * movement (lane crawl) via asymmetric Controller velocity.
 */
class BeatEmUpEditorGameMode : public IEditorGameMode {
public:
  std::string worldSnapshot;
  criogenio::Camera2D editorCamera{};
  criogenio::World *worldForTick = nullptr;
  criogenio::ecs::EntityId playEntity = criogenio::ecs::NULL_ENTITY;
  bool spawnedTransientEntity = false;
  bool renamedForMovement = false;
  std::string originalEntityName;
  bool addedController = false;
  bool addedAnimationState = false;
  bool addedPlayName = false;

  void Start(EditorApp &app, const ProjectContext *project) override;
  void Stop(EditorApp &app, bool revert) override;
  void Tick(float dt) override;
  void DrawHUD() override;
};
