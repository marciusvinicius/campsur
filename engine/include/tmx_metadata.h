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

  void clear() {
    infinite = false;
    boundsMinTx = boundsMinTy = boundsMaxTx = boundsMaxTy = 0;
    mapProperties.clear();
    layerInfo.clear();
    imageLayers.clear();
    drawLayerOrder.clear();
    objectGroups.clear();
  }
};

int TmxGetPropertyInt(const std::vector<TmxProperty> &props, const char *name,
                      int defaultValue);
bool TmxGetPropertyBool(const std::vector<TmxProperty> &props, const char *name,
                        bool defaultValue);
const char *TmxGetPropertyString(const std::vector<TmxProperty> &props,
                                 const char *name, const char *defaultValue);

} // namespace criogenio
