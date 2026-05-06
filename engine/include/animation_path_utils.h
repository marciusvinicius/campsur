#pragma once

#include <string>

namespace criogenio {

/**
 * Resolve a texture path from an animation JSON file: absolute paths, cwd, then
 * common roots derived from the JSON location (`.../data/animations/` → pack root).
 * Strips leading `./` segments so Subterra `player.json` (`./assets/...`) and engine
 * clip exports resolve the same way.
 */
std::string ResolveAnimationTexturePathRelToJson(const std::string &jsonPath,
                                                  std::string texRel);

} // namespace criogenio
