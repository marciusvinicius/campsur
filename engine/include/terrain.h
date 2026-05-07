#pragma once

#include <cstdint>

#include "graphics_types.h"
#include "render.h"
#include "resources.h"
#include "serialization.h"
#include "tmx_metadata.h"
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

// ---- Tiled GID flip-bit encoding ---------------------------------------
// Tiled stores per-tile flips/rotation in the top 3 bits of the 32-bit GID.
// We keep these bits in the engine's `int` cells so they round-trip through
// save/load. Decode with the helpers below; the renderer applies them.
//
// NOTE: stored as `int`. Tiles with the H-flip bit (0x80000000) appear as
// negative values; that's expected.
constexpr uint32_t kTileFlipH = 0x80000000u;
constexpr uint32_t kTileFlipV = 0x40000000u;
constexpr uint32_t kTileFlipD = 0x20000000u; // diagonal (rotate 90 CW + flip H)
constexpr uint32_t kTileGidMask = 0x1FFFFFFFu;
inline uint32_t TileGid(int stored) {
  return static_cast<uint32_t>(stored) & kTileGidMask;
}
inline bool TileFlipH(int stored) {
  return (static_cast<uint32_t>(stored) & kTileFlipH) != 0;
}
inline bool TileFlipV(int stored) {
  return (static_cast<uint32_t>(stored) & kTileFlipV) != 0;
}
inline bool TileFlipD(int stored) {
  return (static_cast<uint32_t>(stored) & kTileFlipD) != 0;
}
/** Combine a base gid with H/V/D flip flags. Use when writing tiles. */
inline int MakeFlippedTile(uint32_t baseGid, bool h, bool v, bool d) {
  uint32_t g = baseGid & kTileGidMask;
  if (h) g |= kTileFlipH;
  if (v) g |= kTileFlipV;
  if (d) g |= kTileFlipD;
  return static_cast<int>(g);
}

struct Tileset {
  std::shared_ptr<TextureResource> atlas;
  std::string tilesetPath;
  int tileSize = 32;
  int columns = 1;
  int rows = 1;
  /** If 0, use tileSize for height (square tiles). */
  int tileHeightPx = 0;
  int margin = 0;
  int spacing = 0;
};

/** One Tiled tileset range (global IDs from firstGid upward). Sorted by firstGid in Terrain2D. */
struct TmxTilesetEntry {
  int firstGid = 1;
  Tileset sheet;
  int tilePixelW = 32;
  int tilePixelH = 32;
  int margin = 0;
  int spacing = 0;
  /**
   * Per-tile custom properties parsed from `<tile id="N"><properties>...</properties></tile>`.
   * Key is the local tile index (0-based within this tileset). Empty when the
   * source TMX/TSX has no per-tile properties. The editor surfaces these
   * read-only when the user picks a tile (eyedropper or palette click).
   */
  std::map<int, std::vector<TmxProperty>> tileProperties;
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
  /** Reorder a layer; clamps `to` into range. */
  void MoveLayer(int from, int to);
  /** Deep-copy `index`'s chunks and insert the copy immediately after it. */
  void DuplicateLayer(int index);
  void SetTileset(const Tileset &newTileset);
  void SetTileSize(int newTileSize);
  SerializedTerrain2D Serialize() const;
  void Deserialize(const SerializedTerrain2D &data,
                   const std::string &asset_root_dir = {});
  void SetAtlas(int layer, const char *path);

  // Visible tile range in world coords (for editor grid / culling)
  void GetVisibleTileRange(const Camera2D &camera, float viewWidth, float viewHeight,
                           int &minTx, int &minTy, int &maxTx, int &maxTy) const;

  int GridStepX() const { return mapTilePxW_ > 0 ? mapTilePxW_ : tileset.tileSize; }
  int GridStepY() const { return mapTilePxH_ > 0 ? mapTilePxH_ : tileset.tileSize; }
  bool UsesGidMode() const { return gidMode_; }
  bool CellHasTile(int layerIndex, int worldTx, int worldTy) const;
  /**
   * When `tmxMeta.collisionSolid` is non-empty, tests axis-aligned rect (world pixels,
   * top-left) against merged TMX collision tiles.
   */
  bool TmxFootprintOverlapsSolid(float rectLeft, float rectTop, float w, float h) const;
  void ClearTmxState();

  /**
   * Rebuild `tmxMeta.collisionSolid` from layers whose name contains "collision" or have
   * property `collides=true`, plus object groups whose name contains "collision".
   * Uses `tmxMeta.bounds*` (tile space, max exclusive) for grid size.
   */
  void RebuildCollisionMaskFromTmxRules();

  /** Set `tmxMeta.bounds*` and logical tile size from the union of all painted tiles. */
  void InferTmxContentBoundsFromTiles();

  /** TMX `<map width="…" height="…">` in tiles; 0 if not from TMX. */
  int LogicalMapWidthTiles() const { return logicalMapTilesW_; }
  int LogicalMapHeightTiles() const { return logicalMapTilesH_; }

  /** Filled by `TilemapLoader::LoadFromTMX` (layer `index`, roof, windows, objects, …). */
  TmxMapMetadata tmxMeta;

  friend std::optional<SerializedLevelMetadata> TerrainExportLevelMetadata(const Terrain2D &,
                                                                           const std::string &);
  friend void TerrainImportLevelMetadata(Terrain2D &, const SerializedLevelMetadata &,
                                         const std::string &);

public:
  Tileset tileset;
  std::vector<ChunkedLayer> layers;
  int chunkSize_ = kDefaultChunkSize;
  std::vector<TmxTilesetEntry> tmxTilesets;

private:
  friend class TilemapLoader;
  void BeginTmxMode(int mapTileW, int mapTileH, std::vector<TmxTilesetEntry> entries);
  void SetTmxLogicalExtent(int tilesW, int tilesH);
  const TmxTilesetEntry *FindTmxTileset(uint32_t gid) const;
  void RenderChunkedLayerGid(Renderer &renderer, const ChunkedLayer &layer,
                             float alpha = 1.0f) const;
  void RenderChunkedLayerTileIndex(Renderer &renderer, const ChunkedLayer &layer,
                                   float alpha = 1.0f) const;

  bool gidMode_ = false;
  int mapTilePxW_ = 0;
  int mapTilePxH_ = 0;
  int logicalMapTilesW_ = 0;
  int logicalMapTilesH_ = 0;
};

class Terrain3D : public Terrain {
public:
  void Render(Renderer &renderer) override;
  void Update(float dt) override;

public:
  void* model = nullptr;   // placeholder for future 3D backend
  void* texture = nullptr; // placeholder for future 3D backend
};

/** Shared helper for top-left anchored entity clamping against map extents. */
void ComputeMovementBoundsPx(const Terrain2D *terrain, float entityW, float entityH,
                             int fallbackTilesW, int fallbackTilesH, int fallbackStepX,
                             int fallbackStepY, float *outMinX, float *outMinY, float *outMaxX,
                             float *outMaxY);

} // namespace criogenio
