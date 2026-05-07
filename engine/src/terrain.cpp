#include "terrain.h"
#include "asset_manager.h"
#include "graphics_types.h"
#include "resources.h"
#include "tmx_metadata.h"
#include <algorithm>
#include <cctype>
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

void ComputeMovementBoundsPx(const Terrain2D *terrain, float entityW, float entityH,
                             int fallbackTilesW, int fallbackTilesH, int fallbackStepX,
                             int fallbackStepY, float *outMinX, float *outMinY, float *outMaxX,
                             float *outMaxY) {
  if (!outMinX || !outMinY || !outMaxX || !outMaxY)
    return;
  float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;

  if (terrain && terrain->UsesGidMode() && terrain->LogicalMapWidthTiles() > 0) {
    const float sx = static_cast<float>(terrain->GridStepX());
    const float sy = static_cast<float>(terrain->GridStepY());
    const auto &m = terrain->tmxMeta;
    minX = static_cast<float>(m.boundsMinTx) * sx;
    minY = static_cast<float>(m.boundsMinTy) * sy;
    maxX = static_cast<float>(m.boundsMaxTx) * sx - entityW;
    maxY = static_cast<float>(m.boundsMaxTy) * sy - entityH;
  } else if (terrain && terrain->LogicalMapWidthTiles() > 0) {
    minX = 0.f;
    minY = 0.f;
    maxX = terrain->LogicalMapWidthTiles() * static_cast<float>(terrain->GridStepX()) - entityW;
    maxY = terrain->LogicalMapHeightTiles() * static_cast<float>(terrain->GridStepY()) - entityH;
  } else {
    minX = minY = 0.f;
    maxX = (fallbackTilesW - 1) * static_cast<float>(fallbackStepX) - entityW;
    maxY = (fallbackTilesH - 1) * static_cast<float>(fallbackStepY) - entityH;
  }

  if (maxX < minX)
    maxX = minX;
  if (maxY < minY)
    maxY = minY;
  *outMinX = minX;
  *outMinY = minY;
  *outMaxX = maxX;
  *outMaxY = maxY;
}

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
                                      const ChunkedLayer &layer,
                                      float alpha) const {
  const int stepX = GridStepX();
  const int stepY = GridStepY();
  Color tint = Colors::White;
  if (alpha < 1.0f) {
    if (alpha < 0.0f) alpha = 0.0f;
    tint.a = static_cast<uint8_t>(alpha * 255.0f);
  }
  for (const auto &[key, tiles] : layer.chunks) {
    int cx = key.first;
    int cy = key.second;
    if (static_cast<int>(tiles.size()) != chunkSize_ * chunkSize_)
      continue;
    for (int ly = 0; ly < chunkSize_; ly++) {
      for (int lx = 0; lx < chunkSize_; lx++) {
        int stored = tiles[ly * chunkSize_ + lx];
        // 0 = empty cell (still 0 with no flip bits). A non-zero stored value
        // may be negative (H-flip sets bit 31); decode the base gid via TileGid.
        if (stored == 0)
          continue;
        const uint32_t gid = TileGid(stored);
        if (gid == 0)
          continue;
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
        const bool fH = TileFlipH(stored);
        const bool fV = TileFlipV(stored);
        // TODO: support diagonal flip (bit 29). Tiled defines it as
        // rotate-90-CW followed by H-flip-of-result; SDL3's flip-then-rotate
        // order requires a small case table to emulate. H+V flips cover the
        // common case (mirror tiles for symmetric maps).
        if (fH || fV) {
          renderer.DrawTextureProFlipped(ts->sheet.atlas->texture, sourceRect,
                                         destRect, {0.f, 0.f}, 0.f, tint, fH, fV);
        } else {
          renderer.DrawTexturePro(ts->sheet.atlas->texture, sourceRect, destRect,
                                  {0.f, 0.f}, 0.f, tint);
        }
      }
    }
  }
}

void Terrain2D::RenderChunkedLayerTileIndex(Renderer &renderer,
                                            const ChunkedLayer &layer,
                                            float alpha) const {
  if (!tileset.atlas)
    return;
  Color tint = Colors::White;
  if (alpha < 1.0f) {
    if (alpha < 0.0f) alpha = 0.0f;
    tint.a = static_cast<uint8_t>(alpha * 255.0f);
  }
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
                                tint);
      }
    }
  }
}

void Terrain2D::Render(Renderer &renderer) {
  // Look up visibility/opacity for layer index `i`. When metadata is missing,
  // layers are visible at full opacity.
  auto layerVisible = [this](size_t i) -> bool {
    return i >= tmxMeta.layerInfo.size() ? true : tmxMeta.layerInfo[i].visible;
  };
  auto layerAlpha = [this](size_t i) -> float {
    return i >= tmxMeta.layerInfo.size() ? 1.0f : tmxMeta.layerInfo[i].opacity;
  };

  if (gidMode_) {
    if (!tmxMeta.drawLayerOrder.empty()) {
      for (const TmxDrawLayerStep &step : tmxMeta.drawLayerOrder) {
        if (step.kind == TmxDrawLayerKind::Tile) {
          if (step.index >= 0 && step.index < static_cast<int>(layers.size())) {
            const size_t li = static_cast<size_t>(step.index);
            if (!layerVisible(li)) continue;
            RenderChunkedLayerGid(renderer, layers[li], layerAlpha(li));
          }
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
      for (size_t i = 0; i < layers.size(); ++i) {
        if (!layerVisible(i)) continue;
        RenderChunkedLayerGid(renderer, layers[i], layerAlpha(i));
      }
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
      for (size_t idx : order) {
        if (!layerVisible(idx)) continue;
        RenderChunkedLayerGid(renderer, layers[idx], layerAlpha(idx));
      }
    }
    return;
  }

  if (!tileset.atlas)
    return;

  const bool sortLayers = tmxMeta.layerInfo.size() == layers.size() &&
                          !tmxMeta.layerInfo.empty();
  if (!sortLayers) {
    for (size_t i = 0; i < layers.size(); ++i) {
      if (!layerVisible(i)) continue;
      RenderChunkedLayerTileIndex(renderer, layers[i], layerAlpha(i));
    }
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
    for (size_t idx : order) {
      if (!layerVisible(idx)) continue;
      RenderChunkedLayerTileIndex(renderer, layers[idx], layerAlpha(idx));
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
  if (gidMode_) {
    // In GID mode, the high 3 bits encode flip flags; a tile is "present"
    // iff the low 29 bits (the actual gid) are non-zero. Comparing v > 0
    // would falsely return false for tiles with the H-flip bit set (which
    // makes the int negative).
    return TileGid(v) != 0;
  }
  return v >= 0;
}

bool Terrain2D::TmxFootprintOverlapsSolid(float rectLeft, float rectTop, float w,
                                        float h) const {
  if (tmxMeta.collisionSolid.empty() || w <= 0.f || h <= 0.f)
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

namespace {

bool asciiLowerContainsSubstring(const std::string &name, const char *needle) {
  if (name.empty())
    return false;
  std::string low;
  low.reserve(name.size());
  for (unsigned char c : name)
    low.push_back(static_cast<char>(std::tolower(c)));
  return low.find(needle) != std::string::npos;
}

bool tiledLayerFeedsCollisionMask(const TiledLayerMeta &meta) {
  if (asciiLowerContainsSubstring(meta.name, "collision"))
    return true;
  return TmxGetPropertyBool(meta.properties, "collides", false);
}

} // namespace

void Terrain2D::RebuildCollisionMaskFromTmxRules() {
  TmxMapMetadata &meta = tmxMeta;
  meta.collisionSolid.clear();
  meta.collisionStrideTiles = 0;
  meta.collisionHeightTiles = 0;
  const int w = meta.boundsMaxTx - meta.boundsMinTx;
  const int h = meta.boundsMaxTy - meta.boundsMinTy;
  if (w <= 0 || h <= 0)
    return;
  meta.collisionStrideTiles = w;
  meta.collisionHeightTiles = h;
  meta.collisionSolid.assign(static_cast<size_t>(w * h), 0);

  auto markTile = [&](int worldTx, int worldTy) {
    const int lx = worldTx - meta.boundsMinTx;
    const int ly = worldTy - meta.boundsMinTy;
    if (lx < 0 || ly < 0 || lx >= w || ly >= h)
      return;
    meta.collisionSolid[static_cast<size_t>(ly * w + lx)] = 1;
  };

  for (size_t li = 0; li < layers.size() && li < meta.layerInfo.size(); ++li) {
    if (!tiledLayerFeedsCollisionMask(meta.layerInfo[li]))
      continue;
    const int layerIdx = static_cast<int>(li);
    for (int ty = meta.boundsMinTy; ty < meta.boundsMaxTy; ++ty) {
      for (int tx = meta.boundsMinTx; tx < meta.boundsMaxTx; ++tx) {
        if (CellHasTile(layerIdx, tx, ty))
          markTile(tx, ty);
      }
    }
  }

  const int gw = GridStepX();
  const int gh = GridStepY();
  if (gw <= 0 || gh <= 0)
    return;
  const float orx = origin.x;
  const float ory = origin.y;

  for (const TiledObjectGroup &og : meta.objectGroups) {
    if (!asciiLowerContainsSubstring(og.name, "collision"))
      continue;
    for (const TiledMapObject &obj : og.objects) {
      if (obj.point)
        continue;
      const float x1 = obj.x;
      const float y1 = obj.y;
      const float x2 = obj.x + obj.width;
      const float y2 = obj.y + obj.height;
      if (obj.width <= 0.f && obj.height <= 0.f)
        continue;
      if (obj.width <= 0.f || obj.height <= 0.f) {
        const int tx =
            static_cast<int>(std::floor((obj.x - orx) / static_cast<float>(gw)));
        const int ty =
            static_cast<int>(std::floor((obj.y - ory) / static_cast<float>(gh)));
        markTile(tx, ty);
        continue;
      }
      const int tminx = static_cast<int>(
          std::floor((std::min(x1, x2) - orx) / static_cast<float>(gw)));
      const int tmaxx = static_cast<int>(
          std::floor((std::max(x1, x2) - orx - 1e-3f) / static_cast<float>(gw)));
      const int tminy = static_cast<int>(
          std::floor((std::min(y1, y2) - ory) / static_cast<float>(gh)));
      const int tmaxy = static_cast<int>(
          std::floor((std::max(y1, y2) - ory - 1e-3f) / static_cast<float>(gh)));
      for (int ty = tminy; ty <= tmaxy; ++ty) {
        for (int tx = tminx; tx <= tmaxx; ++tx)
          markTile(tx, ty);
      }
    }
  }
}

void Terrain2D::InferTmxContentBoundsFromTiles() {
  bool has = false;
  int minTx = 0, minTy = 0, maxTx = 0, maxTy = 0;
  const int n = chunkSize_ * chunkSize_;
  for (size_t li = 0; li < layers.size(); ++li) {
    for (const auto &[key, tiles] : layers[li].chunks) {
      if (static_cast<int>(tiles.size()) != n)
        continue;
      const int cx = key.first;
      const int cy = key.second;
      for (int ly = 0; ly < chunkSize_; ++ly) {
        for (int lx = 0; lx < chunkSize_; ++lx) {
          const int wtx = cx * chunkSize_ + lx;
          const int wty = cy * chunkSize_ + ly;
          if (!CellHasTile(static_cast<int>(li), wtx, wty))
            continue;
          if (!has) {
            minTx = maxTx = wtx;
            minTy = maxTy = wty;
            has = true;
          } else {
            minTx = std::min(minTx, wtx);
            maxTx = std::max(maxTx, wtx);
            minTy = std::min(minTy, wty);
            maxTy = std::max(maxTy, wty);
          }
        }
      }
    }
  }
  if (!has)
    return;
  tmxMeta.boundsMinTx = minTx;
  tmxMeta.boundsMinTy = minTy;
  tmxMeta.boundsMaxTx = maxTx + 1;
  tmxMeta.boundsMaxTy = maxTy + 1;
  logicalMapTilesW_ = tmxMeta.boundsMaxTx - tmxMeta.boundsMinTx;
  logicalMapTilesH_ = tmxMeta.boundsMaxTy - tmxMeta.boundsMinTy;
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
  TiledLayerMeta lm;
  lm.name = "Layer " + std::to_string(tmxMeta.layerInfo.size());
  tmxMeta.layerInfo.push_back(std::move(lm));
}

void Terrain2D::RemoveLayer(int layerIndex) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;
  layers.erase(layers.begin() + layerIndex);
  if (layerIndex < static_cast<int>(tmxMeta.layerInfo.size()))
    tmxMeta.layerInfo.erase(tmxMeta.layerInfo.begin() + layerIndex);
}

void Terrain2D::ClearLayer(int layerIndex) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;
  layers[layerIndex].chunks.clear();
}

void Terrain2D::ClearAllLayers() {
  layers.clear();
}

void Terrain2D::MoveLayer(int from, int to) {
  const int n = static_cast<int>(layers.size());
  if (n <= 1) return;
  if (from < 0 || from >= n) return;
  if (to < 0) to = 0;
  if (to >= n) to = n - 1;
  if (from == to) return;
  // Move the chunked layer.
  ChunkedLayer moved = std::move(layers[from]);
  layers.erase(layers.begin() + from);
  layers.insert(layers.begin() + to, std::move(moved));
  // Reorder TMX layer metadata in lockstep so visibility/opacity follow.
  if (from < static_cast<int>(tmxMeta.layerInfo.size())) {
    int metaTo = std::min(to, static_cast<int>(tmxMeta.layerInfo.size()) - 1);
    if (metaTo < 0) metaTo = 0;
    TiledLayerMeta lm = std::move(tmxMeta.layerInfo[from]);
    tmxMeta.layerInfo.erase(tmxMeta.layerInfo.begin() + from);
    tmxMeta.layerInfo.insert(tmxMeta.layerInfo.begin() + metaTo, std::move(lm));
  }
}

void Terrain2D::DuplicateLayer(int index) {
  const int n = static_cast<int>(layers.size());
  if (index < 0 || index >= n) return;
  ChunkedLayer copy = layers[index]; // deep copy via map<vector<int>>
  layers.insert(layers.begin() + index + 1, std::move(copy));
  if (index < static_cast<int>(tmxMeta.layerInfo.size())) {
    TiledLayerMeta lm = tmxMeta.layerInfo[index];
    lm.name += " (copy)";
    tmxMeta.layerInfo.insert(tmxMeta.layerInfo.begin() + index + 1, std::move(lm));
  }
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
