#include "terrain.h"
#include "asset_manager.h"
#include "graphics_types.h"
#include "resources.h"
#include "tmx_metadata.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

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

void renderTmxImageLayer(Renderer &renderer, const TiledImageLayerMeta &im, Vec2 terrainOrigin) {
  if (!im.visible || !im.image || !im.image->texture.valid())
    return;
  const float x = terrainOrigin.x + im.offsetX;
  const float y = terrainOrigin.y + im.offsetY;
  const float tw = static_cast<float>(im.image->texture.width);
  const float th = static_cast<float>(im.image->texture.height);
  const float dw = im.widthPx > 0 ? static_cast<float>(im.widthPx) : tw;
  const float dh = im.heightPx > 0 ? static_cast<float>(im.heightPx) : th;
  Color tint = Colors::White;
  tint.a = static_cast<unsigned char>(std::clamp(im.opacity, 0.f, 1.f) * 255.f);
  Rect src{0.f, 0.f, tw, th};
  Rect dst{x, y, dw, dh};
  renderer.DrawTexturePro(im.image->texture, src, dst, {0.f, 0.f}, 0.f, tint,
                          TextureBlendMode::Alpha);
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
  float tsx = static_cast<float>(GridStepX());
  float tsy = static_cast<float>(GridStepY());
  Vec2 worldMin = ScreenToWorld2D({0, 0}, camera, viewWidth, viewHeight);
  Vec2 worldMax =
      ScreenToWorld2D({viewWidth, viewHeight}, camera, viewWidth, viewHeight);
  float minWx = std::min(worldMin.x, worldMax.x) - origin.x;
  float minWy = std::min(worldMin.y, worldMax.y) - origin.y;
  float maxWx = std::max(worldMin.x, worldMax.x) - origin.x;
  float maxWy = std::max(worldMin.y, worldMax.y) - origin.y;
  minTx = static_cast<int>(std::floor(minWx / tsx)) - 1;
  minTy = static_cast<int>(std::floor(minWy / tsy)) - 1;
  maxTx = static_cast<int>(std::floor(maxWx / tsx)) + 1;
  maxTy = static_cast<int>(std::floor(maxWy / tsy)) + 1;
}

void Terrain2D::RenderChunkedLayerGid(Renderer &renderer,
                                      const ChunkedLayer &layer) const {
  const int stepX = GridStepX();
  const int stepY = GridStepY();
  for (const auto &[key, tiles] : layer.chunks) {
    int cx = key.first;
    int cy = key.second;
    if (static_cast<int>(tiles.size()) != chunkSize_ * chunkSize_)
      continue;
    for (int ly = 0; ly < chunkSize_; ly++) {
      for (int lx = 0; lx < chunkSize_; lx++) {
        int stored = tiles[ly * chunkSize_ + lx];
        if (stored <= 0)
          continue;
        auto gid = static_cast<uint32_t>(stored);
        const TmxTilesetEntry *ts = FindTmxTileset(gid);
        if (!ts || !ts->sheet.atlas)
          continue;
        int local = static_cast<int>(gid) - ts->firstGid;
        if (local < 0)
          continue;
        int tw = ts->tilePixelW;
        int th = ts->tilePixelH;
        int col = local % ts->sheet.columns;
        int row = local / ts->sheet.columns;
        int m = ts->margin;
        int sp = ts->spacing;
        float sx = static_cast<float>(m + col * (tw + sp));
        float sy = static_cast<float>(m + row * (th + sp));
        Rect sourceRect = {sx, sy, static_cast<float>(tw), static_cast<float>(th)};
        float worldX = static_cast<float>((cx * chunkSize_ + lx) * stepX);
        float worldY = static_cast<float>((cy * chunkSize_ + ly) * stepY);
        Rect destRect = {origin.x + worldX, origin.y + worldY,
                         static_cast<float>(stepX), static_cast<float>(stepY)};
        renderer.DrawTexturePro(ts->sheet.atlas->texture, sourceRect, destRect,
                                {0.f, 0.f}, 0.f, Colors::White);
      }
    }
  }
}

void Terrain2D::RenderChunkedLayerTileIndex(Renderer &renderer,
                                            const ChunkedLayer &layer) const {
  if (!tileset.atlas)
    return;
  const int ts = tileset.tileSize;
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

void Terrain2D::Render(Renderer &renderer) {
  if (gidMode_) {
    if (!tmxMeta.drawLayerOrder.empty()) {
      for (const TmxDrawLayerStep &step : tmxMeta.drawLayerOrder) {
        if (step.kind == TmxDrawLayerKind::Tile) {
          if (step.index >= 0 && step.index < static_cast<int>(layers.size()))
            RenderChunkedLayerGid(renderer, layers[static_cast<size_t>(step.index)]);
        } else {
          if (step.index >= 0 &&
              step.index < static_cast<int>(tmxMeta.imageLayers.size()))
            renderTmxImageLayer(renderer,
                                tmxMeta.imageLayers[static_cast<size_t>(step.index)], origin);
        }
      }
      return;
    }
    const bool sortLayers = tmxMeta.layerInfo.size() == layers.size() &&
                            !tmxMeta.layerInfo.empty();
    if (!sortLayers) {
      for (const auto &layer : layers)
        RenderChunkedLayerGid(renderer, layer);
    } else {
      std::vector<size_t> order(layers.size());
      std::iota(order.begin(), order.end(), 0);
      std::stable_sort(order.begin(), order.end(),
                       [this](size_t a, size_t b) {
                         int ia = tmxMeta.layerInfo[a].mapLayerIndex;
                         int ib = tmxMeta.layerInfo[b].mapLayerIndex;
                         if (ia != ib)
                           return ia < ib;
                         return a < b;
                       });
      for (size_t idx : order)
        RenderChunkedLayerGid(renderer, layers[idx]);
    }
    return;
  }

  if (!tileset.atlas)
    return;

  const bool sortLayers = tmxMeta.layerInfo.size() == layers.size() &&
                          !tmxMeta.layerInfo.empty();
  if (!sortLayers) {
    for (const auto &layer : layers)
      RenderChunkedLayerTileIndex(renderer, layer);
  } else {
    std::vector<size_t> order(layers.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(),
                     [this](size_t a, size_t b) {
                       int ia = tmxMeta.layerInfo[a].mapLayerIndex;
                       int ib = tmxMeta.layerInfo[b].mapLayerIndex;
                       if (ia != ib)
                         return ia < ib;
                       return a < b;
                     });
    for (size_t idx : order)
      RenderChunkedLayerTileIndex(renderer, layers[idx]);
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
    return gidMode_ ? 0 : -1;
  const auto &tiles = it->second;
  if (static_cast<int>(tiles.size()) != chunkSize_ * chunkSize_)
    return gidMode_ ? 0 : -1;
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
  const int emptyFill = gidMode_ ? 0 : -1;
  if (static_cast<int>(tiles.size()) != chunkSize_ * chunkSize_) {
    tiles.assign(chunkSize_ * chunkSize_, emptyFill);
  }
  tiles[ly * chunkSize_ + lx] = tileId;
  return *this;
}

bool Terrain2D::CellHasTile(int layerIndex, int worldTx, int worldTy) const {
  int v = GetTile(layerIndex, worldTx, worldTy);
  if (gidMode_)
    return v > 0;
  return v >= 0;
}

bool Terrain2D::TmxFootprintOverlapsSolid(float rectLeft, float rectTop, float w,
                                        float h) const {
  if (!gidMode_ || tmxMeta.collisionSolid.empty() || w <= 0.f || h <= 0.f)
    return false;
  const int gw = GridStepX();
  const int gh = GridStepY();
  if (gw <= 0 || gh <= 0)
    return false;
  const float ox = origin.x;
  const float oy = origin.y;
  const float r = rectLeft + w;
  const float b = rectTop + h;
  const int minTx = static_cast<int>(std::floor((rectLeft - ox) / static_cast<float>(gw)));
  const int maxTx = static_cast<int>(std::floor((r - ox - 1e-3f) / static_cast<float>(gw)));
  const int minTy = static_cast<int>(std::floor((rectTop - oy) / static_cast<float>(gh)));
  const int maxTy = static_cast<int>(std::floor((b - oy - 1e-3f) / static_cast<float>(gh)));
  for (int ty = minTy; ty <= maxTy; ++ty) {
    for (int tx = minTx; tx <= maxTx; ++tx) {
      if (!tmxMeta.collisionTileSolid(tx, ty))
        continue;
      const float tileL = ox + tx * static_cast<float>(gw);
      const float tileT = oy + ty * static_cast<float>(gh);
      const float tileR = tileL + static_cast<float>(gw);
      const float tileB = tileT + static_cast<float>(gh);
      if (rectLeft < tileR && r > tileL && rectTop < tileB && b > tileT)
        return true;
    }
  }
  return false;
}

void Terrain2D::ClearTmxState() {
  gidMode_ = false;
  mapTilePxW_ = mapTilePxH_ = 0;
  logicalMapTilesW_ = logicalMapTilesH_ = 0;
  tmxTilesets.clear();
  tmxMeta.clear();
}

void Terrain2D::SetTmxLogicalExtent(int tilesW, int tilesH) {
  logicalMapTilesW_ = tilesW;
  logicalMapTilesH_ = tilesH;
}

void Terrain2D::BeginTmxMode(int mapTileW, int mapTileH,
                             std::vector<TmxTilesetEntry> entries) {
  ClearTmxState();
  gidMode_ = true;
  mapTilePxW_ = mapTileW;
  mapTilePxH_ = mapTileH;
  tmxTilesets = std::move(entries);
  if (!tmxTilesets.empty())
    tileset = tmxTilesets[0].sheet;
}

const TmxTilesetEntry *Terrain2D::FindTmxTileset(uint32_t gid) const {
  if (gid == 0)
    return nullptr;
  for (int i = static_cast<int>(tmxTilesets.size()) - 1; i >= 0; --i) {
    if (gid >= static_cast<uint32_t>(tmxTilesets[static_cast<size_t>(i)].firstGid))
      return &tmxTilesets[static_cast<size_t>(i)];
  }
  return nullptr;
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

void Terrain2D::Deserialize(const SerializedTerrain2D &data,
                            const std::string &asset_root_dir) {
  ClearTmxState();
  tileset.tileSize = data.tileset.tileSize;
  tileset.tilesetPath =
      ResolvePathRelativeToWorldFile(asset_root_dir, data.tileset.tilesetPath);
  tileset.atlas =
      criogenio::AssetManager::instance().load<TextureResource>(
          tileset.tilesetPath);
  tileset.columns = data.tileset.columns;
  tileset.rows = data.tileset.rows;
  tileset.tileHeightPx = 0;
  tileset.margin = 0;
  tileset.spacing = 0;

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
