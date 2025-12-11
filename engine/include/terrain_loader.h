
#pragma once
#include "terrain.h"
#include <string>

namespace criogenio {

class TilemapLoader {
public:
  static Terrain2D LoadFromJSON(const std::string &path);
};

} // namespace criogenio
