#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <memory>
#include <optional>
namespace criogenio {

using Variant = std::variant<int, float, bool, std::string>;

inline float GetFloat(const Variant &v) {
  if (std::holds_alternative<float>(v))
    return std::get<float>(v);
  if (std::holds_alternative<int>(v))
    return (float)std::get<int>(v);
  throw std::runtime_error("Expected float");
}

inline int GetInt(const Variant &v) {
  if (std::holds_alternative<int>(v))
    return std::get<int>(v);
  if (std::holds_alternative<float>(v))
    return static_cast<int>(std::get<float>(v));
  throw std::runtime_error("Expected int");
}

inline std::string GetString(const Variant &v) {
  if (std::holds_alternative<std::string>(v))
    return std::get<std::string>(v);
  throw std::runtime_error("Expected string");
}

/** Directory containing a file path (`/a/b/world.json` → `/a/b`). `.` if there is no separator. */
inline std::string DirnameOfFilePath(std::string path) {
  while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
    path.pop_back();
  const auto pos = path.find_last_of("/\\");
  if (pos == std::string::npos)
    return std::string(".");
  return path.substr(0, pos);
}

/**
 * Resolve a path saved in a world file: relative paths are taken from the world JSON's directory;
 * absolute paths (Unix /, Windows drive, UNC) are unchanged.
 */
inline std::string ResolvePathRelativeToWorldFile(const std::string &worldFileDir,
                                                 const std::string &path) {
  if (path.empty())
    return path;
  if (!path.empty() && path[0] == '/')
    return path;
  if (path.size() >= 2 && path[1] == ':' &&
      std::isalpha(static_cast<unsigned char>(path[0])))
    return path;
  if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\')
    return path;
  if (worldFileDir.empty())
    return path;
  std::string root = worldFileDir;
  while (!root.empty() && (root.back() == '/' || root.back() == '\\'))
    root.pop_back();
  return root + "/" + path;
}

struct SerializedComponent {
  std::string type;
  std::unordered_map<std::string, Variant> fields;
};

struct SerializedEntity {
  int id;
  std::string name;
  std::vector<SerializedComponent> components;
};

struct SerializedAnimationFrame {
  float x;
  float y;
  float width;
  float height;
};

struct SerializedAnimationClip {
  std::string name;
  std::vector<SerializedAnimationFrame> frames;
  float frameSpeed;
  // Optional link to an AnimState (stored as int, cast to AnimState)
  int state = 0;
  // Optional link to a Direction (stored as int, cast to Direction)
  int direction = 0;
};

struct SerializedAnimation {
  uint32_t id;
  std::string texturePath;
  std::vector<SerializedAnimationClip> clips;
};

struct SerializedTileLayer {
  int width;
  int height;
  std::vector<int> tiles;
};

struct SerializedChunk {
  int chunkX;
  int chunkY;
  std::vector<int> tiles;
};

struct SerializedChunkedLayer {
  std::vector<SerializedChunk> chunks;
};

struct SerializedTileSet {
  std::string tilesetPath;
  int tileSize;
  int columns;
  int rows;
};

struct SerializedTerrain2D {
  int chunkSize = 0; // 0 = legacy (flat layers), >0 = chunked
  std::vector<SerializedTileLayer> layers;       // legacy format
  std::vector<SerializedChunkedLayer> chunkedLayers; // chunked format when chunkSize > 0
  SerializedTileSet tileset;
};

/** Mirrors `TmxMapMetadata` + TMX terrain mode; embedded in world/level JSON under `"level"`. */
struct SerializedLevelTmxProperty {
  std::string name;
  std::string value;
  std::string type;
};

struct SerializedLevelTiledLayerMeta {
  std::string name;
  int tiledLayerId = -1;
  int mapLayerIndex = 0;
  bool roof = false;
  bool windows = false;
  bool drawAfterEntities = false;
  bool visible = true;
  float opacity = 1.f;
  std::vector<SerializedLevelTmxProperty> properties;
};

struct SerializedLevelMapObject {
  int id = 0;
  std::string name;
  std::string objectType;
  float x = 0.f;
  float y = 0.f;
  float width = 0.f;
  float height = 0.f;
  bool point = false;
  std::vector<SerializedLevelTmxProperty> properties;
};

struct SerializedLevelObjectGroup {
  std::string name;
  int id = -1;
  std::vector<SerializedLevelMapObject> objects;
};

struct SerializedLevelSpawnPrefab {
  float x = 0.f;
  float y = 0.f;
  float width = 0.f;
  float height = 0.f;
  std::string prefabName;
  int quantity = 1;
};

struct SerializedLevelInteractable {
  float x = 0.f;
  float y = 0.f;
  float w = 0.f;
  float h = 0.f;
  bool is_point = false;
  int tiled_object_id = 0;
  std::string interactable_type;
  std::vector<SerializedLevelTmxProperty> properties;
};

struct SerializedLevelImageLayer {
  std::string name;
  int tiledLayerId = -1;
  float offsetX = 0.f;
  float offsetY = 0.f;
  float opacity = 1.f;
  bool visible = true;
  int widthPx = 0;
  int heightPx = 0;
  /** Resolved with world JSON directory on load. */
  std::string imagePath;
  std::vector<SerializedLevelTmxProperty> properties;
};

struct SerializedLevelDrawLayerStep {
  /** 0 = tile layer index, 1 = image layer index (see `TmxDrawLayerKind`). */
  int kind = 0;
  int index = 0;
};

struct SerializedLevelMapLightSource {
  float x = 0.f;
  float y = 0.f;
  float radius = 128.f;
  unsigned char r = 255, g = 180, b = 80;
  bool is_window = false;
};

/** One per-tile property as preserved through level save/load. */
struct SerializedLevelTilePropertyEntry {
  int tileLocalId = 0;
  std::vector<SerializedLevelTmxProperty> properties;
};

struct SerializedLevelTmxTileset {
  int firstGid = 1;
  std::string atlasPath;
  int tilePixelW = 32;
  int tilePixelH = 32;
  int margin = 0;
  int spacing = 0;
  int columns = 1;
  int rows = 1;
  /** Sparse per-tile custom properties parsed from the source TSX/inline tileset. */
  std::vector<SerializedLevelTilePropertyEntry> tileProperties;
};

struct SerializedLevelMetadata {
  bool gidMode = false;
  int mapTilePxW = 0;
  int mapTilePxH = 0;
  int logicalMapTilesW = 0;
  int logicalMapTilesH = 0;
  std::vector<SerializedLevelTmxTileset> tmxTilesets;

  bool infinite = false;
  int boundsMinTx = 0;
  int boundsMinTy = 0;
  int boundsMaxTx = 0;
  int boundsMaxTy = 0;
  std::vector<SerializedLevelTmxProperty> mapProperties;
  std::vector<SerializedLevelTiledLayerMeta> layerInfo;
  std::vector<SerializedLevelImageLayer> imageLayers;
  std::vector<SerializedLevelDrawLayerStep> drawLayerOrder;
  std::vector<SerializedLevelObjectGroup> objectGroups;
  std::vector<SerializedLevelSpawnPrefab> spawnPrefabs;
  std::vector<SerializedLevelInteractable> interactables;
  std::vector<SerializedLevelMapLightSource> mapLightSources;
  int collisionStrideTiles = 0;
  int collisionHeightTiles = 0;
  std::vector<uint8_t> collisionSolid;
};

struct SerializedWorld {
  std::string name;
  std::vector<SerializedEntity> entities;
  SerializedTerrain2D terrain;
  std::vector<SerializedAnimation> animations;
  /** Subterra / TMX-equivalent map metadata (triggers, spawns, collision, …). */
  std::optional<SerializedLevelMetadata> level;
};

} // namespace criogenio
