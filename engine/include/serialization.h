#pragma once

#include <stdexcept>
#include <cstdint>
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
  throw std::runtime_error("Expected int");
}

inline std::string GetString(const Variant &v) {
  if (std::holds_alternative<std::string>(v))
    return std::get<std::string>(v);
  throw std::runtime_error("Expected string");
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
