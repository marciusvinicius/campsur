#pragma once

#include "graphics_types.h"
#include "render.h"
#include "resources.h"
#include "serialization.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace criogenio {

struct TileLayer {
  int width;
  int height;
  std::vector<int> tiles;
};

// Chunked layer: infinite terrain via chunks keyed by (chunkX, chunkY)
using ChunkKey = std::pair<int, int>;
struct ChunkedLayer {
  std::map<ChunkKey, std::vector<int>> chunks; // (cx, cy) -> tiles[chunkSize*chunkSize]
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
  static constexpr int kDefaultChunkSize = 16;

  Vec2 origin = {0, 0}; // world-space anchor
  void Render(Renderer &renderer) override;
  void Update(float dt) override;

  // Chunked terrain API (infinite / large terrain)
  int GetChunkSize() const { return chunkSize_; }
  void SetChunkSize(int size);
  int GetTile(int layerIndex, int worldTx, int worldTy) const;
  Terrain2D &SetTile(int layerIndex, int worldTx, int worldTy, int tileId);

  void FillLayer(int layerIndex, int tileId);
  void AddLayer();           // add empty chunked layer
  void RemoveLayer(int layerIndex);
  void ClearLayer(int layerIndex);
  void ClearAllLayers();
  void SetTileset(const Tileset &newTileset);
  void SetTileSize(int newTileSize);
  SerializedTerrain2D Serialize() const;
  void Deserialize(const SerializedTerrain2D &data);
  void SetAtlas(int layer, const char *path);

  // Visible tile range in world coords (for editor grid / culling)
  void GetVisibleTileRange(const Camera2D &camera, float viewWidth, float viewHeight,
                           int &minTx, int &minTy, int &maxTx, int &maxTy) const;

public:
  Tileset tileset;
  std::vector<ChunkedLayer> layers;
  int chunkSize_ = kDefaultChunkSize;
};

class Terrain3D : public Terrain {
public:
  void Render(Renderer &renderer) override;
  void Update(float dt) override;

public:
  void* model = nullptr;   // placeholder for future 3D backend
  void* texture = nullptr; // placeholder for future 3D backend
};

} // namespace criogenio
