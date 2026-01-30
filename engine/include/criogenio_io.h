#pragma once
#include "world.h"
#include "animation_database.h"

namespace criogenio {

bool SaveWorldToFile(const World &world, const std::string &path);
bool LoadWorldFromFile(World &world, const std::string &path);

// Save a single animation definition (texture + clips) to a standalone JSON
// file so it can be reused by other animation components.
bool SaveAnimationToFile(AssetId animId, const std::string &path);

// Load an animation definition from a standalone JSON file and register it in
// the AnimationDatabase. Returns the created AssetId, or INVALID_ASSET_ID on
// failure.
AssetId LoadAnimationFromFile(const std::string &path);

} // namespace criogenio