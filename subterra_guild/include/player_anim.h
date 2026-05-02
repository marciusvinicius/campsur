#pragma once

#include "animation_database.h"
#include <string>

namespace subterra {

struct PlayerAnimConfig {
  int frameW = 64;
  int frameH = 64;
};

/**
 * Loads `player.json` (Subterra / Tiled format: direction_order, animations with
 * start_row, frame_count, fps) and registers clips named idle_*, walk_*, run_*
 * for use with criogenio::AnimationSystem / RenderSystem.
 *
 * Texture paths in JSON are resolved relative to the project root directory
 * that contains `assets/` (parent of `assets/animations/`).
 */
criogenio::AssetId LoadPlayerAnimationDatabaseFromJson(const std::string &jsonPath,
                                                       PlayerAnimConfig *outCfg = nullptr);

} // namespace subterra
