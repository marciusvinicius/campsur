#pragma once

#include "json.hpp"
#include "serialization.h"

namespace criogenio {

class Terrain2D;

void LevelMetadataToJson(const SerializedLevelMetadata &lm, nlohmann::json &out);
void LevelMetadataFromJson(const nlohmann::json &j, SerializedLevelMetadata &out);

/** Build level JSON from terrain TMX metadata; `worldFileDir` parent of world.json for relative paths. */
std::optional<SerializedLevelMetadata> TerrainExportLevelMetadata(const Terrain2D &terrain,
                                                                 const std::string &worldFileDir);

/** Apply deserialized level metadata after terrain tile data is loaded. */
void TerrainImportLevelMetadata(Terrain2D &terrain, const SerializedLevelMetadata &lm,
                                const std::string &assetRootDir);

} // namespace criogenio
