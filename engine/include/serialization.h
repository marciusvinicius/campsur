#pragma once

#include "error.h"
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

struct SerializedAnimation {
  std::string name;
  std::vector<int> frameIndices;
  float frameSpeed;
};

struct SerializedTileLayer {
  int width;
  int height;
  std::vector<int> tiles;
};

struct SerializedTileSet {
  std::string tilesetPath;
  int tileSize;
  int columns;
  int rows;
};

struct SerializedTerrain2D {
  std::vector<SerializedTileLayer> layers;
  SerializedTileSet tileset;
};

struct SerializedWorld {
  std::string name;
  std::vector<SerializedEntity> entities;
  SerializedTerrain2D terrain;
};

} // namespace criogenio
