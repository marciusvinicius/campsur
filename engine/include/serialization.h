#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <memory>
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

struct SerializedWorld {
  std::string name;
  std::vector<SerializedEntity> entities;
  SerializedTerrain2D terrain;
  std::vector<SerializedAnimation> animations;
};

} // namespace criogenio
