#include "level_metadata_json.h"
#include "asset_manager.h"
#include "terrain.h"
#include "tmx_metadata.h"

#include <algorithm>
#include <cctype>

namespace criogenio {

static std::string PathRelativeToWorldDir(const std::string &worldDir, const std::string &path) {
  if (worldDir.empty() || path.empty())
    return path;
  std::string base = worldDir;
  while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
    base.pop_back();
  if (path.size() > base.size() + 1 && path.compare(0, base.size(), base) == 0) {
    const char sep = path[base.size()];
    if (sep == '/' || sep == '\\')
      return path.substr(base.size() + 1);
  }
  return path;
}

static void PropsToJson(const std::vector<SerializedLevelTmxProperty> &props, nlohmann::json &arr) {
  arr = nlohmann::json::array();
  for (const auto &p : props) {
    nlohmann::json jp;
    jp["name"] = p.name;
    jp["value"] = p.value;
    jp["type"] = p.type;
    arr.push_back(std::move(jp));
  }
}

static void PropsFromJson(const nlohmann::json &arr, std::vector<SerializedLevelTmxProperty> &out) {
  out.clear();
  if (!arr.is_array())
    return;
  for (const auto &jp : arr) {
    if (!jp.is_object())
      continue;
    SerializedLevelTmxProperty p;
    if (jp.contains("name"))
      p.name = jp["name"].get<std::string>();
    if (jp.contains("value"))
      p.value = jp["value"].get<std::string>();
    if (jp.contains("type"))
      p.type = jp["type"].get<std::string>();
    out.push_back(std::move(p));
  }
}

void LevelMetadataToJson(const SerializedLevelMetadata &lm, nlohmann::json &j) {
  j = nlohmann::json::object();
  j["gidMode"] = lm.gidMode;
  j["mapTilePxW"] = lm.mapTilePxW;
  j["mapTilePxH"] = lm.mapTilePxH;
  j["logicalMapTilesW"] = lm.logicalMapTilesW;
  j["logicalMapTilesH"] = lm.logicalMapTilesH;

  nlohmann::json jts = nlohmann::json::array();
  for (const auto &t : lm.tmxTilesets) {
    nlohmann::json jt;
    jt["firstGid"] = t.firstGid;
    jt["atlasPath"] = t.atlasPath;
    jt["tilePixelW"] = t.tilePixelW;
    jt["tilePixelH"] = t.tilePixelH;
    jt["margin"] = t.margin;
    jt["spacing"] = t.spacing;
    jt["columns"] = t.columns;
    jt["rows"] = t.rows;
    if (!t.tileProperties.empty()) {
      nlohmann::json jtp = nlohmann::json::array();
      for (const auto &tp : t.tileProperties) {
        nlohmann::json jentry;
        jentry["tileLocalId"] = tp.tileLocalId;
        PropsToJson(tp.properties, jentry["properties"]);
        jtp.push_back(std::move(jentry));
      }
      jt["tileProperties"] = std::move(jtp);
    }
    jts.push_back(std::move(jt));
  }
  j["tmxTilesets"] = std::move(jts);

  j["infinite"] = lm.infinite;
  j["boundsMinTx"] = lm.boundsMinTx;
  j["boundsMinTy"] = lm.boundsMinTy;
  j["boundsMaxTx"] = lm.boundsMaxTx;
  j["boundsMaxTy"] = lm.boundsMaxTy;

  PropsToJson(lm.mapProperties, j["mapProperties"]);

  nlohmann::json jli = nlohmann::json::array();
  for (const auto &li : lm.layerInfo) {
    nlohmann::json jl;
    jl["name"] = li.name;
    jl["tiledLayerId"] = li.tiledLayerId;
    jl["mapLayerIndex"] = li.mapLayerIndex;
    jl["roof"] = li.roof;
    jl["windows"] = li.windows;
    jl["drawAfterEntities"] = li.drawAfterEntities;
    jl["visible"] = li.visible;
    jl["opacity"] = li.opacity;
    PropsToJson(li.properties, jl["properties"]);
    jli.push_back(std::move(jl));
  }
  j["layerInfo"] = std::move(jli);

  nlohmann::json jim = nlohmann::json::array();
  for (const auto &im : lm.imageLayers) {
    nlohmann::json ji;
    ji["name"] = im.name;
    ji["tiledLayerId"] = im.tiledLayerId;
    ji["offsetX"] = im.offsetX;
    ji["offsetY"] = im.offsetY;
    ji["opacity"] = im.opacity;
    ji["visible"] = im.visible;
    ji["widthPx"] = im.widthPx;
    ji["heightPx"] = im.heightPx;
    ji["imagePath"] = im.imagePath;
    PropsToJson(im.properties, ji["properties"]);
    jim.push_back(std::move(ji));
  }
  j["imageLayers"] = std::move(jim);

  nlohmann::json jdo = nlohmann::json::array();
  for (const auto &d : lm.drawLayerOrder) {
    nlohmann::json jd;
    jd["kind"] = d.kind;
    jd["index"] = d.index;
    jdo.push_back(std::move(jd));
  }
  j["drawLayerOrder"] = std::move(jdo);

  nlohmann::json jog = nlohmann::json::array();
  for (const auto &og : lm.objectGroups) {
    nlohmann::json jg;
    jg["name"] = og.name;
    jg["id"] = og.id;
    nlohmann::json jo = nlohmann::json::array();
    for (const auto &obj : og.objects) {
      nlohmann::json jo0;
      jo0["id"] = obj.id;
      jo0["name"] = obj.name;
      jo0["objectType"] = obj.objectType;
      jo0["x"] = obj.x;
      jo0["y"] = obj.y;
      jo0["width"] = obj.width;
      jo0["height"] = obj.height;
      jo0["point"] = obj.point;
      PropsToJson(obj.properties, jo0["properties"]);
      jo.push_back(std::move(jo0));
    }
    jg["objects"] = std::move(jo);
    jog.push_back(std::move(jg));
  }
  j["objectGroups"] = std::move(jog);

  nlohmann::json jsp = nlohmann::json::array();
  for (const auto &sp : lm.spawnPrefabs) {
    nlohmann::json js;
    js["x"] = sp.x;
    js["y"] = sp.y;
    js["width"] = sp.width;
    js["height"] = sp.height;
    js["prefabName"] = sp.prefabName;
    js["quantity"] = sp.quantity;
    jsp.push_back(std::move(js));
  }
  j["spawnPrefabs"] = std::move(jsp);

  nlohmann::json jia = nlohmann::json::array();
  for (const auto &ia : lm.interactables) {
    nlohmann::json ji;
    ji["x"] = ia.x;
    ji["y"] = ia.y;
    ji["w"] = ia.w;
    ji["h"] = ia.h;
    ji["isPoint"] = ia.is_point;
    ji["tiledObjectId"] = ia.tiled_object_id;
    ji["interactableType"] = ia.interactable_type;
    PropsToJson(ia.properties, ji["properties"]);
    jia.push_back(std::move(ji));
  }
  j["interactables"] = std::move(jia);

  nlohmann::json jls = nlohmann::json::array();
  for (const auto &ls : lm.mapLightSources) {
    nlohmann::json jl;
    jl["x"] = ls.x;
    jl["y"] = ls.y;
    jl["radius"] = ls.radius;
    jl["r"] = ls.r;
    jl["g"] = ls.g;
    jl["b"] = ls.b;
    jl["isWindow"] = ls.is_window;
    jls.push_back(std::move(jl));
  }
  j["mapLightSources"] = std::move(jls);

  j["collisionStrideTiles"] = lm.collisionStrideTiles;
  j["collisionHeightTiles"] = lm.collisionHeightTiles;
  j["collisionSolid"] = lm.collisionSolid;
}

void LevelMetadataFromJson(const nlohmann::json &j, SerializedLevelMetadata &lm) {
  lm = SerializedLevelMetadata{};
  if (!j.is_object())
    return;
  lm.gidMode = j.value("gidMode", false);
  lm.mapTilePxW = j.value("mapTilePxW", 0);
  lm.mapTilePxH = j.value("mapTilePxH", 0);
  lm.logicalMapTilesW = j.value("logicalMapTilesW", 0);
  lm.logicalMapTilesH = j.value("logicalMapTilesH", 0);

  if (j.contains("tmxTilesets") && j["tmxTilesets"].is_array()) {
    for (const auto &jt : j["tmxTilesets"]) {
      if (!jt.is_object())
        continue;
      SerializedLevelTmxTileset t;
      t.firstGid = jt.value("firstGid", 1);
      if (jt.contains("atlasPath"))
        t.atlasPath = jt["atlasPath"].get<std::string>();
      t.tilePixelW = jt.value("tilePixelW", 32);
      t.tilePixelH = jt.value("tilePixelH", 32);
      t.margin = jt.value("margin", 0);
      t.spacing = jt.value("spacing", 0);
      t.columns = jt.value("columns", 1);
      t.rows = jt.value("rows", 1);
      if (jt.contains("tileProperties") && jt["tileProperties"].is_array()) {
        for (const auto &jentry : jt["tileProperties"]) {
          if (!jentry.is_object()) continue;
          SerializedLevelTilePropertyEntry tp;
          tp.tileLocalId = jentry.value("tileLocalId", 0);
          if (jentry.contains("properties"))
            PropsFromJson(jentry["properties"], tp.properties);
          t.tileProperties.push_back(std::move(tp));
        }
      }
      lm.tmxTilesets.push_back(std::move(t));
    }
  }

  lm.infinite = j.value("infinite", false);
  lm.boundsMinTx = j.value("boundsMinTx", 0);
  lm.boundsMinTy = j.value("boundsMinTy", 0);
  lm.boundsMaxTx = j.value("boundsMaxTx", 0);
  lm.boundsMaxTy = j.value("boundsMaxTy", 0);

  if (j.contains("mapProperties"))
    PropsFromJson(j["mapProperties"], lm.mapProperties);

  if (j.contains("layerInfo") && j["layerInfo"].is_array()) {
    for (const auto &jl : j["layerInfo"]) {
      if (!jl.is_object())
        continue;
      SerializedLevelTiledLayerMeta li;
      li.name = jl.value("name", std::string());
      li.tiledLayerId = jl.value("tiledLayerId", -1);
      li.mapLayerIndex = jl.value("mapLayerIndex", 0);
      li.roof = jl.value("roof", false);
      li.windows = jl.value("windows", false);
      li.drawAfterEntities = jl.value("drawAfterEntities", false);
      li.visible = jl.value("visible", true);
      li.opacity = jl.value("opacity", 1.f);
      if (jl.contains("properties"))
        PropsFromJson(jl["properties"], li.properties);
      lm.layerInfo.push_back(std::move(li));
    }
  }

  if (j.contains("imageLayers") && j["imageLayers"].is_array()) {
    for (const auto &ji : j["imageLayers"]) {
      if (!ji.is_object())
        continue;
      SerializedLevelImageLayer im;
      im.name = ji.value("name", std::string());
      im.tiledLayerId = ji.value("tiledLayerId", -1);
      im.offsetX = ji.value("offsetX", 0.f);
      im.offsetY = ji.value("offsetY", 0.f);
      im.opacity = ji.value("opacity", 1.f);
      im.visible = ji.value("visible", true);
      im.widthPx = ji.value("widthPx", 0);
      im.heightPx = ji.value("heightPx", 0);
      if (ji.contains("imagePath"))
        im.imagePath = ji["imagePath"].get<std::string>();
      if (ji.contains("properties"))
        PropsFromJson(ji["properties"], im.properties);
      lm.imageLayers.push_back(std::move(im));
    }
  }

  if (j.contains("drawLayerOrder") && j["drawLayerOrder"].is_array()) {
    for (const auto &jd : j["drawLayerOrder"]) {
      if (!jd.is_object())
        continue;
      SerializedLevelDrawLayerStep s;
      s.kind = jd.value("kind", 0);
      s.index = jd.value("index", 0);
      lm.drawLayerOrder.push_back(s);
    }
  }

  if (j.contains("objectGroups") && j["objectGroups"].is_array()) {
    for (const auto &jg : j["objectGroups"]) {
      if (!jg.is_object())
        continue;
      SerializedLevelObjectGroup og;
      og.name = jg.value("name", std::string());
      og.id = jg.value("id", -1);
      if (jg.contains("objects") && jg["objects"].is_array()) {
        for (const auto &jo : jg["objects"]) {
          if (!jo.is_object())
            continue;
          SerializedLevelMapObject obj;
          obj.id = jo.value("id", 0);
          obj.name = jo.value("name", std::string());
          obj.objectType = jo.value("objectType", std::string());
          obj.x = jo.value("x", 0.f);
          obj.y = jo.value("y", 0.f);
          obj.width = jo.value("width", 0.f);
          obj.height = jo.value("height", 0.f);
          obj.point = jo.value("point", false);
          if (jo.contains("properties"))
            PropsFromJson(jo["properties"], obj.properties);
          og.objects.push_back(std::move(obj));
        }
      }
      lm.objectGroups.push_back(std::move(og));
    }
  }

  if (j.contains("spawnPrefabs") && j["spawnPrefabs"].is_array()) {
    for (const auto &js : j["spawnPrefabs"]) {
      if (!js.is_object())
        continue;
      SerializedLevelSpawnPrefab sp;
      sp.x = js.value("x", 0.f);
      sp.y = js.value("y", 0.f);
      sp.width = js.value("width", 0.f);
      sp.height = js.value("height", 0.f);
      sp.prefabName = js.value("prefabName", std::string());
      sp.quantity = js.value("quantity", 1);
      lm.spawnPrefabs.push_back(std::move(sp));
    }
  }

  if (j.contains("interactables") && j["interactables"].is_array()) {
    for (const auto &ji : j["interactables"]) {
      if (!ji.is_object())
        continue;
      SerializedLevelInteractable ia;
      ia.x = ji.value("x", 0.f);
      ia.y = ji.value("y", 0.f);
      ia.w = ji.value("w", 0.f);
      ia.h = ji.value("h", 0.f);
      ia.is_point = ji.value("isPoint", false);
      ia.tiled_object_id = ji.value("tiledObjectId", 0);
      ia.interactable_type = ji.value("interactableType", std::string());
      if (ji.contains("properties"))
        PropsFromJson(ji["properties"], ia.properties);
      lm.interactables.push_back(std::move(ia));
    }
  }

  if (j.contains("mapLightSources") && j["mapLightSources"].is_array()) {
    for (const auto &jl : j["mapLightSources"]) {
      if (!jl.is_object())
        continue;
      SerializedLevelMapLightSource ls;
      ls.x = jl.value("x", 0.f);
      ls.y = jl.value("y", 0.f);
      ls.radius = jl.value("radius", 128.f);
      ls.r = static_cast<unsigned char>(jl.value("r", 255));
      ls.g = static_cast<unsigned char>(jl.value("g", 180));
      ls.b = static_cast<unsigned char>(jl.value("b", 80));
      ls.is_window = jl.value("isWindow", false);
      lm.mapLightSources.push_back(std::move(ls));
    }
  }

  lm.collisionStrideTiles = j.value("collisionStrideTiles", 0);
  lm.collisionHeightTiles = j.value("collisionHeightTiles", 0);
  if (j.contains("collisionSolid") && j["collisionSolid"].is_array()) {
    for (const auto &v : j["collisionSolid"]) {
      if (v.is_number_unsigned())
        lm.collisionSolid.push_back(static_cast<uint8_t>(v.get<unsigned>()));
      else if (v.is_number_integer())
        lm.collisionSolid.push_back(static_cast<uint8_t>(v.get<int>()));
    }
  }
}

static TmxProperty toTmxProp(const SerializedLevelTmxProperty &p) {
  TmxProperty o;
  o.name = p.name;
  o.value = p.value;
  o.type = p.type;
  return o;
}

static SerializedLevelTmxProperty fromTmxProp(const TmxProperty &p) {
  SerializedLevelTmxProperty o;
  o.name = p.name;
  o.value = p.value;
  o.type = p.type;
  return o;
}

std::optional<SerializedLevelMetadata> TerrainExportLevelMetadata(const Terrain2D &terrain,
                                                                 const std::string &worldFileDir) {
  const TmxMapMetadata &m = terrain.tmxMeta;
  const bool hasMeta =
      terrain.gidMode_ || m.infinite || (m.boundsMaxTx > m.boundsMinTx && m.boundsMaxTy > m.boundsMinTy) ||
      !m.mapProperties.empty() || !m.layerInfo.empty() || !m.imageLayers.empty() ||
      !m.drawLayerOrder.empty() || !m.objectGroups.empty() || !m.spawnPrefabs.empty() ||
      !m.interactables.empty() || !m.mapLightSources.empty() || !m.collisionSolid.empty();
  if (!hasMeta)
    return std::nullopt;

  SerializedLevelMetadata lm;
  lm.gidMode = terrain.gidMode_;
  lm.mapTilePxW = terrain.mapTilePxW_;
  lm.mapTilePxH = terrain.mapTilePxH_;
  lm.logicalMapTilesW = terrain.logicalMapTilesW_;
  lm.logicalMapTilesH = terrain.logicalMapTilesH_;

  for (const auto &e : terrain.tmxTilesets) {
    SerializedLevelTmxTileset t;
    t.firstGid = e.firstGid;
    t.atlasPath = PathRelativeToWorldDir(worldFileDir, e.sheet.tilesetPath);
    t.tilePixelW = e.tilePixelW;
    t.tilePixelH = e.tilePixelH;
    t.margin = e.margin;
    t.spacing = e.spacing;
    t.columns = e.sheet.columns;
    t.rows = e.sheet.rows;
    for (const auto &tp : e.tileProperties) {
      SerializedLevelTilePropertyEntry s;
      s.tileLocalId = tp.first;
      for (const auto &pr : tp.second)
        s.properties.push_back(fromTmxProp(pr));
      t.tileProperties.push_back(std::move(s));
    }
    lm.tmxTilesets.push_back(std::move(t));
  }

  lm.infinite = m.infinite;
  lm.boundsMinTx = m.boundsMinTx;
  lm.boundsMinTy = m.boundsMinTy;
  lm.boundsMaxTx = m.boundsMaxTx;
  lm.boundsMaxTy = m.boundsMaxTy;
  for (const auto &p : m.mapProperties)
    lm.mapProperties.push_back(fromTmxProp(p));
  for (const auto &li : m.layerInfo) {
    SerializedLevelTiledLayerMeta s;
    s.name = li.name;
    s.tiledLayerId = li.tiledLayerId;
    s.mapLayerIndex = li.mapLayerIndex;
    s.roof = li.roof;
    s.windows = li.windows;
    s.drawAfterEntities = li.drawAfterEntities;
    s.visible = li.visible;
    s.opacity = li.opacity;
    for (const auto &p : li.properties)
      s.properties.push_back(fromTmxProp(p));
    lm.layerInfo.push_back(std::move(s));
  }
  for (const auto &im : m.imageLayers) {
    SerializedLevelImageLayer s;
    s.name = im.name;
    s.tiledLayerId = im.tiledLayerId;
    s.offsetX = im.offsetX;
    s.offsetY = im.offsetY;
    s.opacity = im.opacity;
    s.visible = im.visible;
    s.widthPx = im.widthPx;
    s.heightPx = im.heightPx;
    if (im.image)
      s.imagePath = PathRelativeToWorldDir(worldFileDir, im.image->path);
    for (const auto &p : im.properties)
      s.properties.push_back(fromTmxProp(p));
    lm.imageLayers.push_back(std::move(s));
  }
  for (const auto &d : m.drawLayerOrder) {
    SerializedLevelDrawLayerStep s;
    s.kind = static_cast<int>(d.kind);
    s.index = d.index;
    lm.drawLayerOrder.push_back(s);
  }
  for (const auto &og : m.objectGroups) {
    SerializedLevelObjectGroup g;
    g.name = og.name;
    g.id = og.id;
    for (const auto &obj : og.objects) {
      SerializedLevelMapObject o;
      o.id = obj.id;
      o.name = obj.name;
      o.objectType = obj.objectType;
      o.x = obj.x;
      o.y = obj.y;
      o.width = obj.width;
      o.height = obj.height;
      o.point = obj.point;
      for (const auto &p : obj.properties)
        o.properties.push_back(fromTmxProp(p));
      g.objects.push_back(std::move(o));
    }
    lm.objectGroups.push_back(std::move(g));
  }
  for (const auto &sp : m.spawnPrefabs) {
    SerializedLevelSpawnPrefab s;
    s.x = sp.x;
    s.y = sp.y;
    s.width = sp.width;
    s.height = sp.height;
    s.prefabName = sp.prefabName;
    s.quantity = sp.quantity;
    lm.spawnPrefabs.push_back(std::move(s));
  }
  for (const auto &ia : m.interactables) {
    SerializedLevelInteractable s;
    s.x = ia.x;
    s.y = ia.y;
    s.w = ia.w;
    s.h = ia.h;
    s.is_point = ia.is_point;
    s.tiled_object_id = ia.tiled_object_id;
    s.interactable_type = ia.interactable_type;
    for (const auto &p : ia.properties)
      s.properties.push_back(fromTmxProp(p));
    lm.interactables.push_back(std::move(s));
  }
  for (const auto &ls : m.mapLightSources) {
    SerializedLevelMapLightSource s;
    s.x = ls.x;
    s.y = ls.y;
    s.radius = ls.radius;
    s.r = ls.r;
    s.g = ls.g;
    s.b = ls.b;
    s.is_window = ls.is_window;
    lm.mapLightSources.push_back(std::move(s));
  }
  lm.collisionStrideTiles = m.collisionStrideTiles;
  lm.collisionHeightTiles = m.collisionHeightTiles;
  lm.collisionSolid = m.collisionSolid;
  return lm;
}

void TerrainImportLevelMetadata(Terrain2D &terrain, const SerializedLevelMetadata &lm,
                                const std::string &assetRootDir) {
  TmxMapMetadata m;
  m.infinite = lm.infinite;
  m.boundsMinTx = lm.boundsMinTx;
  m.boundsMinTy = lm.boundsMinTy;
  m.boundsMaxTx = lm.boundsMaxTx;
  m.boundsMaxTy = lm.boundsMaxTy;
  for (const auto &p : lm.mapProperties)
    m.mapProperties.push_back(toTmxProp(p));
  for (const auto &li : lm.layerInfo) {
    TiledLayerMeta t;
    t.name = li.name;
    t.tiledLayerId = li.tiledLayerId;
    t.mapLayerIndex = li.mapLayerIndex;
    t.roof = li.roof;
    t.windows = li.windows;
    t.drawAfterEntities = li.drawAfterEntities;
    t.visible = li.visible;
    t.opacity = li.opacity;
    for (const auto &p : li.properties)
      t.properties.push_back(toTmxProp(p));
    m.layerInfo.push_back(std::move(t));
  }
  for (const auto &im : lm.imageLayers) {
    TiledImageLayerMeta t;
    t.name = im.name;
    t.tiledLayerId = im.tiledLayerId;
    t.offsetX = im.offsetX;
    t.offsetY = im.offsetY;
    t.opacity = im.opacity;
    t.visible = im.visible;
    t.widthPx = im.widthPx;
    t.heightPx = im.heightPx;
    for (const auto &p : im.properties)
      t.properties.push_back(toTmxProp(p));
    if (!im.imagePath.empty()) {
      const std::string resolved = ResolvePathRelativeToWorldFile(assetRootDir, im.imagePath);
      t.image = AssetManager::instance().load<TextureResource>(resolved);
    }
    m.imageLayers.push_back(std::move(t));
  }
  for (const auto &d : lm.drawLayerOrder) {
    TmxDrawLayerStep s;
    s.kind = (d.kind == 1) ? TmxDrawLayerKind::Image : TmxDrawLayerKind::Tile;
    s.index = d.index;
    m.drawLayerOrder.push_back(s);
  }
  for (const auto &og : lm.objectGroups) {
    TiledObjectGroup g;
    g.name = og.name;
    g.id = og.id;
    for (const auto &obj : og.objects) {
      TiledMapObject o;
      o.id = obj.id;
      o.name = obj.name;
      o.objectType = obj.objectType;
      o.x = obj.x;
      o.y = obj.y;
      o.width = obj.width;
      o.height = obj.height;
      o.point = obj.point;
      for (const auto &p : obj.properties)
        o.properties.push_back(toTmxProp(p));
      g.objects.push_back(std::move(o));
    }
    m.objectGroups.push_back(std::move(g));
  }
  for (const auto &sp : lm.spawnPrefabs) {
    TiledSpawnPrefab t;
    t.x = sp.x;
    t.y = sp.y;
    t.width = sp.width;
    t.height = sp.height;
    t.prefabName = sp.prefabName;
    t.quantity = sp.quantity;
    m.spawnPrefabs.push_back(std::move(t));
  }
  for (const auto &ia : lm.interactables) {
    TiledInteractable t;
    t.x = ia.x;
    t.y = ia.y;
    t.w = ia.w;
    t.h = ia.h;
    t.is_point = ia.is_point;
    t.tiled_object_id = ia.tiled_object_id;
    t.interactable_type = ia.interactable_type;
    for (const auto &p : ia.properties)
      t.properties.push_back(toTmxProp(p));
    m.interactables.push_back(std::move(t));
  }
  for (const auto &ls : lm.mapLightSources) {
    TmxMapMetadata::MapLightSource t;
    t.x = ls.x;
    t.y = ls.y;
    t.radius = ls.radius;
    t.r = ls.r;
    t.g = ls.g;
    t.b = ls.b;
    t.is_window = ls.is_window;
    m.mapLightSources.push_back(t);
  }
  m.collisionStrideTiles = lm.collisionStrideTiles;
  m.collisionHeightTiles = lm.collisionHeightTiles;
  m.collisionSolid = lm.collisionSolid;

  terrain.tmxMeta = std::move(m);
  terrain.gidMode_ = lm.gidMode;
  terrain.mapTilePxW_ = lm.mapTilePxW;
  terrain.mapTilePxH_ = lm.mapTilePxH;
  terrain.logicalMapTilesW_ = lm.logicalMapTilesW;
  terrain.logicalMapTilesH_ = lm.logicalMapTilesH;

  terrain.tmxTilesets.clear();
  for (const auto &e : lm.tmxTilesets) {
    TmxTilesetEntry te;
    te.firstGid = e.firstGid;
    te.margin = e.margin;
    te.spacing = e.spacing;
    te.tilePixelW = e.tilePixelW;
    te.tilePixelH = e.tilePixelH;
    te.sheet.tileSize = e.tilePixelW;
    te.sheet.tileHeightPx = e.tilePixelH;
    te.sheet.margin = e.margin;
    te.sheet.spacing = e.spacing;
    te.sheet.columns = e.columns;
    te.sheet.rows = e.rows;
    te.sheet.tilesetPath = ResolvePathRelativeToWorldFile(assetRootDir, e.atlasPath);
    te.sheet.atlas = AssetManager::instance().load<TextureResource>(te.sheet.tilesetPath);
    for (const auto &tp : e.tileProperties) {
      std::vector<TmxProperty> props;
      for (const auto &pr : tp.properties)
        props.push_back(toTmxProp(pr));
      te.tileProperties.emplace(tp.tileLocalId, std::move(props));
    }
    terrain.tmxTilesets.push_back(std::move(te));
  }
  if (terrain.gidMode_ && !terrain.tmxTilesets.empty())
    terrain.tileset = terrain.tmxTilesets[0].sheet;
}

} // namespace criogenio
