#pragma once

#include "resources.h"
#include <memory>
#include <string>
#include <vector>

namespace criogenio {

/** Single Tiled `<property name=… value=… type=…/>` entry (raw `value` string). */
struct TmxProperty {
  std::string name;
  std::string value;
  /** Tiled `type` when present, e.g. `bool`, `int`, `float`, `string`, `color`. */
  std::string type;
};

struct TiledLayerMeta {
  std::string name;
  /** Tiled layer `id` attribute, or -1 if missing. */
  int tiledLayerId = -1;
  /**
   * Tiled custom property `index`: draw order vs entities (Subterra convention).
   * Lower draws earlier; 0 = same plane as entities.
   */
  int mapLayerIndex = 0;
  bool roof = false;
  bool windows = false;
  /** Convenience: true when `mapLayerIndex > 0` (typical roof/foreground). */
  bool drawAfterEntities = false;
  std::vector<TmxProperty> properties;
};

struct TiledMapObject {
  int id = 0;
  std::string name;
  std::string objectType;
  float x = 0.f;
  float y = 0.f;
  float width = 0.f;
  float height = 0.f;
  bool point = false;
  std::vector<TmxProperty> properties;
};

struct TiledObjectGroup {
  std::string name;
  int id = -1;
  std::vector<TiledMapObject> objects;
};

/** Tiled object layer entry: `type` or `name` `spawn_prefab` with property `spawn_prefab` / `prefab_name`. */
struct TiledSpawnPrefab {
  float x = 0.f;
  float y = 0.f;
  float width = 0.f;
  float height = 0.f;
  std::string prefabName;
  int quantity = 1;
};

/**
 * Tiled object whose `type` or `name` is `interactable` (case-insensitive), with
 * property `interactable_type` or `interactable` giving the kind (door, chest, …).
 */
struct TiledInteractable {
  float x = 0.f;
  float y = 0.f;
  float w = 0.f;
  float h = 0.f;
  bool is_point = false;
  int tiled_object_id = 0;
  std::string interactable_type;
  /** Raw Tiled properties (e.g. `opens_door` on a lever). */
  std::vector<TmxProperty> properties;
};

/** Tiled `<imagelayer>`: full-image decoration between or above tile layers. */
struct TiledImageLayerMeta {
  std::string name;
  int tiledLayerId = -1;
  float offsetX = 0.f;
  float offsetY = 0.f;
  float opacity = 1.f;
  bool visible = true;
  /** Pixel size from `<image>`; if zero, use loaded texture size when drawing. */
  int widthPx = 0;
  int heightPx = 0;
  std::shared_ptr<TextureResource> image;
  std::vector<TmxProperty> properties;
};

enum class TmxDrawLayerKind : uint8_t { Tile = 0, Image = 1 };

/** Document order of tile vs image layers (used when any image layer exists). */
struct TmxDrawLayerStep {
  TmxDrawLayerKind kind = TmxDrawLayerKind::Tile;
  int index = 0;
};

struct TmxMapMetadata {
  bool infinite = false;
  /** Axis-aligned tile bounds in map/chunk coordinates (max is exclusive). */
  int boundsMinTx = 0;
  int boundsMinTy = 0;
  int boundsMaxTx = 0;
  int boundsMaxTy = 0;

  std::vector<TmxProperty> mapProperties;
  /** One entry per tile layer, same order as `Terrain2D::layers`. */
  std::vector<TiledLayerMeta> layerInfo;
  std::vector<TiledImageLayerMeta> imageLayers;
  /**
   * When non-empty, `Terrain2D::Render` draws tile and image layers in this order
   * (Tiled file order). Otherwise tile layers use `layerInfo[].mapLayerIndex` only.
   */
  std::vector<TmxDrawLayerStep> drawLayerOrder;
  std::vector<TiledObjectGroup> objectGroups;
  /** Filled from object layers (spawn_prefab); used by gameplay to place pickups. */
  std::vector<TiledSpawnPrefab> spawnPrefabs;
  /** Filled from object layers (`interactable` type/name + kind property). */
  std::vector<TiledInteractable> interactables;
  /**
   * Static map lights: `windows` tile layers (warm per-tile emitters) and object-layer
   * torches (`type`/`class`/`name` torch or `torch` property), matching reference TMX load.
   */
  struct MapLightSource {
    float x = 0.f;
    float y = 0.f;
    float radius = 128.f;
    unsigned char r = 255, g = 180, b = 80;
    bool is_window = false;
  };
  std::vector<MapLightSource> mapLightSources;

  /**
   * Tile-aligned collision mask (aligned with Odin `TileMap.collision`): layers whose name
   * contains "collision", tile layers with property `collides=true`, and rectangles from
   * object groups whose name contains "collision". Index: `(ty - boundsMinTy) * collisionStrideTiles + (tx - boundsMinTx)`.
   */
  int collisionStrideTiles = 0;
  int collisionHeightTiles = 0;
  std::vector<uint8_t> collisionSolid;

  void clear() {
    infinite = false;
    boundsMinTx = boundsMinTy = boundsMaxTx = boundsMaxTy = 0;
    mapProperties.clear();
    layerInfo.clear();
    imageLayers.clear();
    drawLayerOrder.clear();
    objectGroups.clear();
    spawnPrefabs.clear();
    interactables.clear();
    mapLightSources.clear();
    collisionStrideTiles = 0;
    collisionHeightTiles = 0;
    collisionSolid.clear();
  }

  /** World tile coords (same space as `Terrain2D::GetTile`). */
  bool collisionTileSolid(int worldTx, int worldTy) const {
    if (collisionSolid.empty() || collisionStrideTiles <= 0 || collisionHeightTiles <= 0)
      return false;
    const int lx = worldTx - boundsMinTx;
    const int ly = worldTy - boundsMinTy;
    if (lx < 0 || ly < 0 || lx >= collisionStrideTiles || ly >= collisionHeightTiles)
      return false;
    return collisionSolid[static_cast<size_t>(ly * collisionStrideTiles + lx)] != 0;
  }
};

int TmxGetPropertyInt(const std::vector<TmxProperty> &props, const char *name,
                      int defaultValue);
bool TmxGetPropertyBool(const std::vector<TmxProperty> &props, const char *name,
                        bool defaultValue);
const char *TmxGetPropertyString(const std::vector<TmxProperty> &props,
                                 const char *name, const char *defaultValue);

} // namespace criogenio
