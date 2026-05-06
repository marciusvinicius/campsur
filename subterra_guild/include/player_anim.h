#pragma once

#include "animation_database.h"
#include <string>

namespace subterra {

struct PlayerAnimConfig {
  int frameW = 64;
  int frameH = 64;
};

/**
 * Loads player animation JSON: **Subterra** strip format (`animations` + `start_row`,
 * `male_sheet` / `female_sheet`) or **engine** clip export (`texturePath` + `clips`).
 * Clips idle_*, walk_*, … integrate with criogenio movement/animation systems.
 *
 * Texture paths are resolved via `ResolveAnimationTexturePathRelToJson` (cwd + pack roots).
 */
criogenio::AssetId LoadPlayerAnimationDatabaseFromJson(const std::string &jsonPath,
                                                       PlayerAnimConfig *outCfg = nullptr);

} // namespace subterra
