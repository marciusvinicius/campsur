#pragma once

#include "animation_database.h"
#include <string>

namespace criogenio {

/**
 * Load Subterra Guild-style `player.json` (row strips + direction_order) into
 * AnimationDatabase. Produces clip names `idle_up`, `walk_left`, `jump_down`, …
 * matching AnimationSystem::BuildClipKey.
 *
 * Texture paths in JSON are resolved relative to the folder containing
 * `subterra_guild` (the parent of `data/animations/`).
 */
AssetId LoadSubterraPlayerAnimationJson(const std::string &jsonPath, int *outFrameW = nullptr,
                                        int *outFrameH = nullptr, std::string *errOut = nullptr);

/**
 * Load any Subterra Guild animation JSON from disk: engine clip export (`texturePath` + `clips`),
 * row-strip sheets (`player.json`, `axe.json`, …), or `data/animations` asset placeholders
 * (`texture` + top-level `frames[]` with tile indices).
 */
AssetId LoadSubterraGuildAnimationJson(const std::string &jsonPath, int *outFrameW = nullptr,
                                       int *outFrameH = nullptr, std::string *errOut = nullptr);

/**
 * Export an animation to Subterra `player.json` when clips are compatible:
 * same frame size, four directions per state, horizontal strip (x = i * frameW),
 * rows spaced by one row per direction_order step from `idle_up` (etc.).
 * States: idle, walk, run, jump, attack (AnimState subset).
 */
bool SaveSubterraPlayerAnimationJson(AssetId animId, const std::string &jsonPath,
                                     std::string *errOut = nullptr);

} // namespace criogenio
