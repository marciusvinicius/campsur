#pragma once

#include "animation_database.h"
#include <string>

namespace criogenio {

/**
 * Load Subterra Guild-style `player.json` (row strips + direction_order) into
 * AnimationDatabase.
 *
 * - **Mapped** `animations[].name`: `idle`, `walk`, `run`, `jump`, `attack` → clips
 *   `idle_up`, `walk_down`, … matching `AnimationSystem` / movement.
 * - **Extra** names (`spell_cast`, `death`, `rest`, …) → clips `<name>_<dir>` with
 *   `AnimState::Count` so they are not picked by automatic movement resolution but
 *   remain available via `getClip(animId, "spell_cast_down")` or scripted playback.
 *
 * Texture paths (`male_sheet`, `female_sheet`, or `texture`) resolve in order:
 * cwd if relative path exists, then `data/animations/`, `data/`, pack root
 * (e.g. `subterra_guild/`), then repo root (parent of pack), each joined with the
 * JSON-relative path (e.g. `assets/sprites/...`).
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
