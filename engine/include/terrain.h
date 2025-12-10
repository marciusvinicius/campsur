#pragma once

#include "raylib.h"
#include "render.h"
#include <string>
#include <vector>

namespace criogenio {

struct TileLayer {
  int width;
  int height;
  std::vector<int> tiles;
};

struct Tileset {
  Texture2D atlas;
  int tileSize;
  int columns;
  int rows;
};

// Think about the 3D terrain

class Terrain {
public:
  void Render(Renderer &renderer);
};

class Terrain2D : Terrain {
public:
  void Render(Renderer &renderer);

public:
  Tileset tileset;
  std::vector<TileLayer> layers;
};

class Terrain3D : Terrain {
public:
  void Render(Renderer &renderer);

public:
  Model model;
  Texture2D texture;
};

} // namespace criogenio
