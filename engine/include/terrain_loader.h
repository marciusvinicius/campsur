
#pragma once
#include "terrain.h"
#include <string>

namespace criogenio {

class TilemapLoader {
public:
  static Terrain2D LoadFromJSON(const std::string &path);
  /** Orthogonal TMX, CSV tile layers, external or inline tilesets (.tsx / embedded). */
  static Terrain2D LoadFromTMX(const std::string &path);
};

} // namespace criogenio
