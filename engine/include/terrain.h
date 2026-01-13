#pragma once

#include "raylib.h"
#include "render.h"
#include "resources.h"
#include "serialization.h"
#include <optional>
#include <string>
#include <vector>

namespace criogenio {

struct TileLayer {
  int width;
  int height;
  std::vector<int> tiles;
};

struct Tileset {
  std::shared_ptr<TextureResource> atlas;
  std::string tilesetPath;
  int tileSize;
  int columns;
  int rows;
};

// Think about the 3D terrain

class Terrain {
public:
  virtual void Update(float dt) = 0;
  virtual void Render(Renderer &renderer) = 0;
};

class Terrain2D : public Terrain {
public:
  Vector2 origin = {0, 0}; // world-space anchor
  void Render(Renderer &renderer) override;
  void Update(float dt) override;
  void FillLayer(int layerIndex, int tileId);
  void ResizeLayer(int layerIndex, int newWidth, int newHeight);
  void AddLayer(int width, int height, int defaultTileId = 0);
  void RemoveLayer(int layerIndex);
  void ClearLayer(int layerIndex);
  void ClearAllLayers();
  Terrain2D &SetTile(int layerIndex, int x, int y, int tileId);
  void SetTileset(const Tileset &newTileset);
  void SetTileSize(int newTileSize);
  SerializedTerrain2D Serialize() const;
  void Deserialize(const SerializedTerrain2D &data);

public:
  Tileset tileset;
  std::vector<TileLayer> layers;
};

class Terrain3D : public Terrain {
public:
  void Render(Renderer &renderer) override;
  void Update(float dt) override;

public:
  Model model;
  Texture2D texture;
};

} // namespace criogenio
