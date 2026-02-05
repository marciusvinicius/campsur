#include "terrain.h"
#include "asset_manager.h"
#include "graphics_types.h"
#include <cmath>

namespace criogenio {

namespace {

void worldToChunkLocal(int worldTx, int worldTy, int chunkSize, int &outCx,
                       int &outCy, int &outLx, int &outLy) {
  if (chunkSize <= 0) {
    outCx = outCy = outLx = outLy = 0;
    return;
  }
  outCx = worldTx >= 0 ? worldTx / chunkSize : (worldTx - chunkSize + 1) / chunkSize;
  outCy = worldTy >= 0 ? worldTy / chunkSize : (worldTy - chunkSize + 1) / chunkSize;
  outLx = worldTx - outCx * chunkSize;
  outLy = worldTy - outCy * chunkSize;
  if (outLx < 0)
    outLx += chunkSize;
  if (outLy < 0)
    outLy += chunkSize;
}

} // namespace

void Terrain3D::Update(float dt) {}

void Terrain2D::Update(float dt) {}

void Terrain2D::SetAtlas(int layer, const char *path) {
  tileset.atlas =
      AssetManager::instance().load<criogenio::TextureResource>(path);
  tileset.tilesetPath = path;
}

void Terrain2D::SetChunkSize(int size) {
  if (size < 1)
    size = 1;
  chunkSize_ = size;
}

void Terrain2D::GetVisibleTileRange(const Camera2D &camera, float viewWidth,
                                    float viewHeight, int &minTx, int &minTy,
                                    int &maxTx, int &maxTy) const {
  float ts = static_cast<float>(tileset.tileSize);
  Vec2 worldMin = ScreenToWorld2D({0, 0}, camera, viewWidth, viewHeight);
  Vec2 worldMax =
      ScreenToWorld2D({viewWidth, viewHeight}, camera, viewWidth, viewHeight);
  minTx = static_cast<int>(std::floor(std::min(worldMin.x, worldMax.x) / ts)) - 1;
  minTy = static_cast<int>(std::floor(std::min(worldMin.y, worldMax.y) / ts)) - 1;
  maxTx = static_cast<int>(std::floor(std::max(worldMin.x, worldMax.x) / ts)) + 1;
  maxTy = static_cast<int>(std::floor(std::max(worldMin.y, worldMax.y) / ts)) + 1;
}

void Terrain2D::Render(Renderer &renderer) {
  if (!tileset.atlas)
    return;

  const int ts = tileset.tileSize;
  for (const auto &layer : layers) {
    for (const auto &[key, tiles] : layer.chunks) {
      int cx = key.first;
      int cy = key.second;
      int n = chunkSize_ * chunkSize_;
      if (static_cast<int>(tiles.size()) != n)
        continue;
      for (int ly = 0; ly < chunkSize_; ly++) {
        for (int lx = 0; lx < chunkSize_; lx++) {
          int tileIndex = tiles[ly * chunkSize_ + lx];
          if (tileIndex < 0)
            continue;
          int worldX = (cx * chunkSize_ + lx) * ts;
          int worldY = (cy * chunkSize_ + ly) * ts;
          int tileX = (tileIndex % tileset.columns) * ts;
          int tileY = (tileIndex / tileset.columns) * ts;
          Rect sourceRect = {static_cast<float>(tileX),
                             static_cast<float>(tileY),
                             static_cast<float>(ts), static_cast<float>(ts)};
          Vec2 position = {origin.x + static_cast<float>(worldX),
                           origin.y + static_cast<float>(worldY)};
          renderer.DrawTextureRec(tileset.atlas->texture, sourceRect, position,
                                  Colors::White);
        }
      }
    }
  }
}

void Terrain3D::Render(Renderer &renderer) {
  (void)renderer;
  // 3D rendering not implemented in SDL/abstract backend yet
}

int Terrain2D::GetTile(int layerIndex, int worldTx, int worldTy) const {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return -1;
  int cx, cy, lx, ly;
  worldToChunkLocal(worldTx, worldTy, chunkSize_, cx, cy, lx, ly);
  ChunkKey key{cx, cy};
  const auto &layer = layers[layerIndex];
  auto it = layer.chunks.find(key);
  if (it == layer.chunks.end())
    return -1;
  const auto &tiles = it->second;
  if (static_cast<int>(tiles.size()) != chunkSize_ * chunkSize_)
    return -1;
  return tiles[ly * chunkSize_ + lx];
}

Terrain2D &Terrain2D::SetTile(int layerIndex, int worldTx, int worldTy,
                              int tileId) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return *this;
  int cx, cy, lx, ly;
  worldToChunkLocal(worldTx, worldTy, chunkSize_, cx, cy, lx, ly);
  ChunkKey key{cx, cy};
  auto &layer = layers[layerIndex];
  auto &tiles = layer.chunks[key];
  if (static_cast<int>(tiles.size()) != chunkSize_ * chunkSize_) {
    tiles.resize(chunkSize_ * chunkSize_, -1);
  }
  tiles[ly * chunkSize_ + lx] = tileId;
  return *this;
}

void Terrain2D::FillLayer(int layerIndex, int tileId) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;
  for (auto &[key, tiles] : layers[layerIndex].chunks) {
    std::fill(tiles.begin(), tiles.end(), tileId);
  }
}

void Terrain2D::AddLayer() {
  layers.push_back(ChunkedLayer{});
}

void Terrain2D::RemoveLayer(int layerIndex) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;
  layers.erase(layers.begin() + layerIndex);
}

void Terrain2D::ClearLayer(int layerIndex) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;
  layers[layerIndex].chunks.clear();
}

void Terrain2D::ClearAllLayers() {
  layers.clear();
}

void Terrain2D::SetTileset(const Tileset &newTileset) { tileset = newTileset; }

void Terrain2D::SetTileSize(int newTileSize) {
  tileset.tileSize = newTileSize;
}

SerializedTerrain2D Terrain2D::Serialize() const {
  SerializedTerrain2D out;
  out.chunkSize = chunkSize_;
  out.tileset.tileSize = tileset.tileSize;
  out.tileset.tilesetPath = tileset.tilesetPath;
  out.tileset.columns = tileset.columns;
  out.tileset.rows = tileset.rows;

  for (const auto &layer : layers) {
    SerializedChunkedLayer sl;
    for (const auto &[key, tiles] : layer.chunks) {
      SerializedChunk sc;
      sc.chunkX = key.first;
      sc.chunkY = key.second;
      sc.tiles = tiles;
      sl.chunks.push_back(sc);
    }
    out.chunkedLayers.push_back(sl);
  }
  return out;
}

void Terrain2D::Deserialize(const SerializedTerrain2D &data) {
  tileset.tileSize = data.tileset.tileSize;
  tileset.tilesetPath = data.tileset.tilesetPath;
  tileset.atlas =
      criogenio::AssetManager::instance().load<TextureResource>(
          tileset.tilesetPath);
  tileset.columns = data.tileset.columns;
  tileset.rows = data.tileset.rows;

  chunkSize_ = data.chunkSize > 0 ? data.chunkSize : kDefaultChunkSize;
  layers.clear();

  if (data.chunkSize > 0 && !data.chunkedLayers.empty()) {
    for (const auto &cl : data.chunkedLayers) {
      ChunkedLayer layer;
      for (const auto &sc : cl.chunks) {
        ChunkKey key{sc.chunkX, sc.chunkY};
        layer.chunks[key] = sc.tiles;
      }
      layers.push_back(std::move(layer));
    }
    return;
  }

  // Legacy: flat layers
  for (const auto &layerData : data.layers) {
    ChunkedLayer layer;
    int w = layerData.width;
    int h = layerData.height;
    if (w <= 0 || h <= 0)
      continue;
    int cs = chunkSize_;
    for (int cy = 0; cy * cs < h; cy++) {
      for (int cx = 0; cx * cs < w; cx++) {
        std::vector<int> tiles(cs * cs, -1);
        for (int ly = 0; ly < cs && cy * cs + ly < h; ly++) {
          for (int lx = 0; lx < cs && cx * cs + lx < w; lx++) {
            int tw = cx * cs + lx;
            int th = cy * cs + ly;
            tiles[ly * cs + lx] = layerData.tiles[th * w + tw];
          }
        }
        layer.chunks[{cx, cy}] = std::move(tiles);
      }
    }
    layers.push_back(std::move(layer));
  }
}

} // namespace criogenio
