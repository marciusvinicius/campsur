#include "terrain_loader.h"
#include "asset_manager.h"
#include "resources.h"
#include "terrain.h"
#include "tmx_metadata.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if __has_include(<zlib.h>)
#include <zlib.h>
#define CRIogenio_TMX_ZLIB 1
#endif

namespace criogenio {
namespace {

namespace fs = std::filesystem;

std::string readAll(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    throw std::runtime_error("TMX: cannot open file: " + path);
  return std::string(std::istreambuf_iterator<char>(f), {});
}

std::string resolvePath(const std::string &baseFile, const std::string &relative) {
  fs::path base(baseFile);
  fs::path dir = base.parent_path();
  return (dir / fs::path(relative)).lexically_normal().string();
}

std::string attrValue(std::string_view tag, std::string_view key) {
  std::string q = std::string(key) + "=\"";
  size_t p = tag.find(q);
  if (p == std::string::npos)
    return {};
  p += q.size();
  size_t e = tag.find('"', p);
  if (e == std::string::npos)
    return {};
  return std::string(tag.substr(p, e - p));
}

int parseIntAttr(std::string_view tag, std::string_view key, int def = 0) {
  std::string v = attrValue(tag, key);
  if (v.empty())
    return def;
  return std::stoi(v);
}

float parseFloatAttr(std::string_view tag, std::string_view key, float def = 0.f) {
  std::string v = attrValue(tag, key);
  if (v.empty())
    return def;
  try {
    return std::stof(v);
  } catch (...) {
    return def;
  }
}

std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(0, 1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

std::vector<uint32_t> parseCsvGids(const std::string &body, int expected) {
  std::vector<uint32_t> out;
  out.reserve(static_cast<size_t>(expected));
  std::string token;
  for (char c : body) {
    if (c == ',' || c == '\n' || c == '\r') {
      std::string t = trim(token);
      token.clear();
      if (t.empty())
        continue;
      out.push_back(static_cast<uint32_t>(std::stoull(t)));
    } else
      token += c;
  }
  std::string t = trim(token);
  if (!t.empty())
    out.push_back(static_cast<uint32_t>(std::stoull(t)));
  if (static_cast<int>(out.size()) != expected)
    throw std::runtime_error("TMX: tile data count " + std::to_string(out.size()) +
                             " != " + std::to_string(expected));
  return out;
}

static int base64Value(unsigned char c) {
  if (c >= 'A' && c <= 'Z')
    return static_cast<int>(c - 'A');
  if (c >= 'a' && c <= 'z')
    return static_cast<int>(c - 'a' + 26);
  if (c >= '0' && c <= '9')
    return static_cast<int>(c - '0' + 52);
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return -1;
}

static std::vector<uint8_t> decodeBase64(const std::string &in) {
  std::vector<uint8_t> out;
  unsigned int buf = 0;
  int nbits = 0;
  for (unsigned char ch : in) {
    if (std::isspace(ch))
      continue;
    if (ch == '=')
      break;
    int v = base64Value(ch);
    if (v < 0)
      continue;
    buf = (buf << 6) | static_cast<unsigned int>(v);
    nbits += 6;
    if (nbits >= 8) {
      nbits -= 8;
      out.push_back(static_cast<uint8_t>((buf >> nbits) & 0xFFu));
    }
  }
  return out;
}

#ifdef CRIogenio_TMX_ZLIB
static std::vector<uint8_t> zlibInflate(const uint8_t *src, size_t srcLen, size_t hintOut) {
  uLongf cap = static_cast<uLongf>(hintOut > 4096 ? hintOut : size_t(4096));
  for (int attempt = 0; attempt < 16; ++attempt) {
    std::vector<uint8_t> dest(cap);
    uLongf destLen = cap;
    int zr = uncompress(dest.data(), &destLen, src, static_cast<uLong>(srcLen));
    if (zr == Z_OK) {
      dest.resize(static_cast<size_t>(destLen));
      return dest;
    }
    if (zr != Z_BUF_ERROR)
      throw std::runtime_error("TMX: zlib decompress failed");
    cap *= 2;
  }
  throw std::runtime_error("TMX: zlib output buffer exceeded");
}
#endif

static std::vector<uint32_t> gidsFromLittleEndianBytes(const std::vector<uint8_t> &bin,
                                                     int count) {
  if (static_cast<int>(bin.size()) < count * 4)
    throw std::runtime_error("TMX: decoded tile blob too short (expected " +
                             std::to_string(count * 4) + " bytes, got " +
                             std::to_string(bin.size()) + ")");
  std::vector<uint32_t> gids;
  gids.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    size_t o = static_cast<size_t>(i * 4);
    uint32_t v = static_cast<uint32_t>(bin[o]) |
                 (static_cast<uint32_t>(bin[o + 1]) << 8) |
                 (static_cast<uint32_t>(bin[o + 2]) << 16) |
                 (static_cast<uint32_t>(bin[o + 3]) << 24);
    gids.push_back(v);
  }
  return gids;
}

static std::string toLowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

static std::vector<uint32_t> decodeLayerDataGids(const std::string &dataOpen,
                                                const std::string &dataBody, int lw, int lh) {
  const int expected = lw * lh;
  std::string enc = toLowerAscii(attrValue(dataOpen, "encoding"));
  std::string comp = toLowerAscii(attrValue(dataOpen, "compression"));
  if (enc.empty() || enc == "csv") {
    if (!comp.empty())
      throw std::runtime_error("TMX: compressed tile layer requires encoding=\"base64\"");
    return parseCsvGids(dataBody, expected);
  }
  if (enc != "base64")
    throw std::runtime_error("TMX: unsupported tile layer encoding: " + enc);
  std::vector<uint8_t> bin = decodeBase64(dataBody);
  if (!comp.empty()) {
#ifdef CRIogenio_TMX_ZLIB
    if (comp == "zlib")
      bin = zlibInflate(bin.data(), bin.size(), static_cast<size_t>(expected) * 4u);
    else if (comp == "gzip")
      throw std::runtime_error(
          "TMX: gzip tile compression is not supported (re-export with zlib or CSV)");
    else
      throw std::runtime_error("TMX: unsupported tile compression: " + comp);
#else
    (void)expected;
    throw std::runtime_error(
        "TMX: compressed tile layers need zlib (install zlib headers and rebuild)");
#endif
  }
  return gidsFromLittleEndianBytes(bin, expected);
}

static std::vector<uint32_t> decodeChunkGids(const std::string &inner, int cw, int ch) {
  const int n = cw * ch;
  std::string t = trim(inner);
  if (t.find(',') != std::string::npos)
    return parseCsvGids(t, n);
  std::vector<uint8_t> bin = decodeBase64(t);
#ifdef CRIogenio_TMX_ZLIB
  if (bin.size() >= 2 && bin[0] == 0x78 &&
      (bin[1] == 0x9C || bin[1] == 0x01 || bin[1] == 0xDA))
    bin = zlibInflate(bin.data(), bin.size(), static_cast<size_t>(n) * 4u);
#endif
  return gidsFromLittleEndianBytes(bin, n);
}

void parsePropertyTags(const std::string &xml, size_t start, size_t end,
                       std::vector<TmxProperty> &out) {
  size_t pos = start;
  while (pos < end && (pos = xml.find("<property", pos)) != std::string::npos) {
    if (pos >= end)
      break;
    size_t gt = xml.find('>', pos);
    if (gt == std::string::npos || gt > end)
      break;
    std::string tag = xml.substr(pos, gt - pos + 1);
    TmxProperty p;
    p.name = attrValue(tag, "name");
    p.value = attrValue(tag, "value");
    p.type = attrValue(tag, "type");
    out.push_back(std::move(p));
    pos = gt + 1;
  }
}

void parsePropertiesBlock(const std::string &xml, size_t regionStart, size_t regionEnd,
                          std::vector<TmxProperty> &out) {
  size_t p = xml.find("<properties", regionStart);
  if (p == std::string::npos || p >= regionEnd)
    return;
  size_t close = xml.find("</properties>", p);
  if (close == std::string::npos || close > regionEnd)
    return;
  parsePropertyTags(xml, p, close, out);
}

static void parseOneImagelayer(const std::string &mapPath, const std::string &xml, size_t pos,
                               TiledImageLayerMeta &meta, size_t &outNextScan) {
  size_t openEnd = xml.find('>', pos);
  if (openEnd == std::string::npos)
    throw std::runtime_error("TMX: malformed <imagelayer>");
  std::string openTag = xml.substr(pos, openEnd - pos + 1);
  meta.name = attrValue(openTag, "name");
  meta.tiledLayerId = parseIntAttr(openTag, "id", -1);
  meta.offsetX = parseFloatAttr(openTag, "offsetx", 0.f);
  meta.offsetY = parseFloatAttr(openTag, "offsety", 0.f);
  meta.opacity = parseFloatAttr(openTag, "opacity", 1.f);
  meta.visible = attrValue(openTag, "visible") != "0";

  size_t close = xml.find("</imagelayer>", openEnd);
  if (close == std::string::npos)
    throw std::runtime_error("TMX: unclosed <imagelayer>");
  std::string inner = xml.substr(openEnd + 1, close - (openEnd + 1));
  parsePropertiesBlock(inner, 0, inner.size(), meta.properties);

  size_t imgPos = inner.find("<image");
  if (imgPos == std::string::npos)
    throw std::runtime_error("TMX: imagelayer without <image>");
  size_t imgGt = inner.find('>', imgPos);
  if (imgGt == std::string::npos)
    throw std::runtime_error("TMX: malformed <image> in imagelayer");
  std::string imgTag = inner.substr(imgPos, imgGt - imgPos + 1);
  std::string src = attrValue(imgTag, "source");
  if (src.empty())
    throw std::runtime_error("TMX: imagelayer <image> missing source");
  meta.widthPx = parseIntAttr(imgTag, "width", 0);
  meta.heightPx = parseIntAttr(imgTag, "height", 0);
  std::string imgPath = resolvePath(mapPath, src);
  meta.image = AssetManager::instance().load<TextureResource>(imgPath);
  outNextScan = close + 14;
}

TiledLayerMeta buildLayerMeta(const std::string &layerTag,
                              const std::vector<TmxProperty> &props) {
  TiledLayerMeta m;
  m.name = attrValue(layerTag, "name");
  m.tiledLayerId = parseIntAttr(layerTag, "id", -1);
  m.mapLayerIndex = TmxGetPropertyInt(props, "index", 0);
  m.roof = TmxGetPropertyBool(props, "roof", false);
  m.windows = TmxGetPropertyBool(props, "windows", false);
  m.drawAfterEntities = m.mapLayerIndex > 0;
  m.properties = props;
  return m;
}

struct TileBounds {
  bool has = false;
  int minTx = 0, minTy = 0, maxTx = 0, maxTy = 0;
  void add(int tx, int ty) {
    if (!has) {
      minTx = tx;
      minTy = ty;
      maxTx = tx + 1;
      maxTy = ty + 1;
      has = true;
      return;
    }
    minTx = std::min(minTx, tx);
    minTy = std::min(minTy, ty);
    maxTx = std::max(maxTx, tx + 1);
    maxTy = std::max(maxTy, ty + 1);
  }
  void merge(const TileBounds &o) {
    if (!o.has)
      return;
    if (!has) {
      *this = o;
      return;
    }
    minTx = std::min(minTx, o.minTx);
    minTy = std::min(minTy, o.minTy);
    maxTx = std::max(maxTx, o.maxTx);
    maxTy = std::max(maxTy, o.maxTy);
  }
};

void parseOneObject(const std::string &inner, size_t op, TiledMapObject &obj) {
  size_t gt = inner.find('>', op);
  if (gt == std::string::npos)
    return;
  std::string objOpen = inner.substr(op, gt - op + 1);
  const bool selfClosing =
      objOpen.find("/>") != std::string::npos ||
      (objOpen.size() >= 2 && objOpen[objOpen.size() - 2] == '/' &&
       objOpen.back() == '>');

  obj.id = parseIntAttr(objOpen, "id", 0);
  obj.name = attrValue(objOpen, "name");
  obj.objectType = attrValue(objOpen, "type");
  obj.x = parseFloatAttr(objOpen, "x", 0.f);
  obj.y = parseFloatAttr(objOpen, "y", 0.f);
  obj.width = parseFloatAttr(objOpen, "width", 0.f);
  obj.height = parseFloatAttr(objOpen, "height", 0.f);
  obj.point = (attrValue(objOpen, "point") == "true");
  if (obj.point) {
    obj.width = 0.f;
    obj.height = 0.f;
  }

  if (!selfClosing) {
    size_t objEnd = inner.find("</object>", gt);
    if (objEnd != std::string::npos)
      parsePropertiesBlock(inner, gt + 1, objEnd, obj.properties);
  }
}

void parseObjectGroups(const std::string &xml, std::vector<TiledObjectGroup> &groups) {
  size_t pos = 0;
  while ((pos = xml.find("<objectgroup", pos)) != std::string::npos) {
    size_t gt = xml.find('>', pos);
    if (gt == std::string::npos)
      break;
    std::string openTag = xml.substr(pos, gt - pos + 1);
    size_t close = xml.find("</objectgroup>", gt);
    if (close == std::string::npos)
      break;
    std::string inner = xml.substr(gt + 1, close - (gt + 1));

    TiledObjectGroup og;
    og.name = attrValue(openTag, "name");
    og.id = parseIntAttr(openTag, "id", -1);

    size_t op = 0;
    while ((op = inner.find("<object", op)) != std::string::npos) {
      TiledMapObject obj;
      parseOneObject(inner, op, obj);
      og.objects.push_back(std::move(obj));
      size_t next = inner.find("</object>", op);
      if (next != std::string::npos) {
        op = next + 9;
        continue;
      }
      size_t sc = inner.find("/>", op);
      if (sc != std::string::npos) {
        op = sc + 2;
        continue;
      }
      op += 7;
    }

    groups.push_back(std::move(og));
    pos = close + 14;
  }
}

TmxTilesetEntry loadTsx(const std::string &tsxPath, int firstGid) {
  std::string xml = readAll(tsxPath);
  size_t p = xml.find("<tileset");
  if (p == std::string::npos)
    throw std::runtime_error("TMX: no <tileset> in " + tsxPath);
  size_t gt = xml.find('>', p);
  if (gt == std::string::npos)
    throw std::runtime_error("TMX: malformed tileset tag in " + tsxPath);
  std::string openTag = std::string(xml.substr(p, gt - p + 1));
  int tw = parseIntAttr(openTag, "tilewidth", 32);
  int th = parseIntAttr(openTag, "tileheight", 32);
  int margin = parseIntAttr(openTag, "margin", 0);
  int spacing = parseIntAttr(openTag, "spacing", 0);
  int columns = parseIntAttr(openTag, "columns", 1);
  int tilecount = parseIntAttr(openTag, "tilecount", 0);

  size_t ip = xml.find("<image", gt);
  if (ip == std::string::npos)
    throw std::runtime_error("TMX: no <image> in " + tsxPath);
  size_t igt = xml.find('>', ip);
  std::string imgTag = xml.substr(ip, igt - ip + 1);
  std::string imgSrc = attrValue(imgTag, "source");
  if (imgSrc.empty())
    throw std::runtime_error("TMX: image without source in " + tsxPath);

  std::string imgPath = resolvePath(tsxPath, imgSrc);
  TmxTilesetEntry e{};
  e.firstGid = firstGid;
  e.margin = margin;
  e.spacing = spacing;
  e.tilePixelW = tw;
  e.tilePixelH = th;
  e.sheet.tileSize = tw;
  e.sheet.tileHeightPx = th;
  e.sheet.margin = margin;
  e.sheet.spacing = spacing;
  e.sheet.columns = columns;
  e.sheet.rows =
      (tilecount > 0 && columns > 0) ? (tilecount + columns - 1) / columns : 1;
  e.sheet.tilesetPath = imgPath;
  e.sheet.atlas = AssetManager::instance().load<TextureResource>(imgPath);
  return e;
}

TmxTilesetEntry loadInlineTileset(const std::string &mapPath, std::string_view openTag,
                                  const std::string &innerXml) {
  int firstGid = parseIntAttr(openTag, "firstgid", 0);
  if (firstGid <= 0)
    throw std::runtime_error("TMX: inline tileset missing firstgid");
  int tw = parseIntAttr(openTag, "tilewidth", 32);
  int th = parseIntAttr(openTag, "tileheight", 32);
  int margin = parseIntAttr(openTag, "margin", 0);
  int spacing = parseIntAttr(openTag, "spacing", 0);
  int columns = parseIntAttr(openTag, "columns", 1);
  int tilecount = parseIntAttr(openTag, "tilecount", 0);

  size_t ip = innerXml.find("<image");
  if (ip == std::string::npos)
    throw std::runtime_error("TMX: inline tileset without <image>");
  size_t igt = innerXml.find('>', ip);
  std::string imgTag = innerXml.substr(ip, igt - ip + 1);
  std::string imgSrc = attrValue(imgTag, "source");
  if (imgSrc.empty())
    throw std::runtime_error("TMX: inline tileset image missing source");

  std::string imgPath = resolvePath(mapPath, imgSrc);
  TmxTilesetEntry e{};
  e.firstGid = firstGid;
  e.margin = margin;
  e.spacing = spacing;
  e.tilePixelW = tw;
  e.tilePixelH = th;
  e.sheet.tileSize = tw;
  e.sheet.tileHeightPx = th;
  e.sheet.margin = margin;
  e.sheet.spacing = spacing;
  e.sheet.columns = columns;
  e.sheet.rows =
      (tilecount > 0 && columns > 0) ? (tilecount + columns - 1) / columns : 1;
  e.sheet.tilesetPath = imgPath;
  e.sheet.atlas = AssetManager::instance().load<TextureResource>(imgPath);
  return e;
}

} // namespace

Terrain2D TilemapLoader::LoadFromTMX(const std::string &path) {
  std::string xml = readAll(path);
  size_t mapPos = xml.find("<map");
  if (mapPos == std::string::npos)
    throw std::runtime_error("TMX: no <map> root");
  size_t mapGt = xml.find('>', mapPos);
  std::string mapTag = std::string(xml.substr(mapPos, mapGt - mapPos + 1));
  if (attrValue(mapTag, "orientation") != "orthogonal")
    throw std::runtime_error("TMX: only orthogonal maps supported");

  const bool mapInfinite = (attrValue(mapTag, "infinite") == "1");
  int mapW = parseIntAttr(mapTag, "width", 0);
  int mapH = parseIntAttr(mapTag, "height", 0);
  int tileW = parseIntAttr(mapTag, "tilewidth", 0);
  int tileH = parseIntAttr(mapTag, "tileheight", 0);
  if (!mapInfinite && (mapW <= 0 || mapH <= 0 || tileW <= 0 || tileH <= 0))
    throw std::runtime_error("TMX: invalid map dimensions");
  if (mapInfinite && (tileW <= 0 || tileH <= 0))
    throw std::runtime_error("TMX: infinite map missing tile dimensions");

  size_t firstTileset = xml.find("<tileset", mapGt);
  if (firstTileset == std::string::npos)
    firstTileset = xml.size();
  size_t mapPropRegionStart = mapGt + 1;

  std::vector<TmxTilesetEntry> entries;
  size_t scan = 0;
  while ((scan = xml.find("<tileset", scan)) != std::string::npos) {
    size_t gt = xml.find('>', scan);
    if (gt == std::string::npos)
      throw std::runtime_error("TMX: unclosed <tileset>");
    std::string openTag = std::string(xml.substr(scan, gt - scan + 1));
    std::string src = attrValue(openTag, "source");
    int firstGid = parseIntAttr(openTag, "firstgid", 0);
    if (firstGid <= 0)
      throw std::runtime_error("TMX: tileset missing firstgid");

    if (!src.empty()) {
      std::string tsxPath = resolvePath(path, src);
      entries.push_back(loadTsx(tsxPath, firstGid));
      scan = gt + 1;
      continue;
    }

    size_t tclose = xml.find("</tileset>", gt);
    if (tclose == std::string::npos)
      throw std::runtime_error("TMX: inline <tileset> without </tileset>");
    std::string inner = std::string(xml.substr(gt + 1, tclose - (gt + 1)));
    entries.push_back(loadInlineTileset(path, openTag, inner));
    scan = tclose + 10;
  }

  if (entries.empty())
    throw std::runtime_error("TMX: no tilesets");
  std::sort(entries.begin(), entries.end(),
            [](const TmxTilesetEntry &a, const TmxTilesetEntry &b) {
              return a.firstGid < b.firstGid;
            });

  Terrain2D terrain;
  terrain.SetChunkSize(Terrain2D::kDefaultChunkSize);
  terrain.ClearAllLayers();
  terrain.BeginTmxMode(tileW, tileH, std::move(entries));
  terrain.tmxMeta.infinite = mapInfinite;
  parsePropertiesBlock(xml, mapPropRegionStart, firstTileset,
                       terrain.tmxMeta.mapProperties);

  TileBounds layerBounds;

  std::vector<std::pair<size_t, int>> tileLayerFilePos;

  size_t pos = 0;
  while ((pos = xml.find("<layer", pos)) != std::string::npos) {
    const size_t layerXmlPos = pos;
    size_t openEnd = xml.find('>', pos);
    if (openEnd == std::string::npos)
      break;
    std::string layerTag = std::string(xml.substr(pos, openEnd - pos + 1));

    size_t layerClose = xml.find("</layer>", openEnd);
    if (layerClose == std::string::npos)
      throw std::runtime_error("TMX: unclosed <layer>");

    size_t dataPos = xml.find("<data", openEnd);
    if (dataPos == std::string::npos || dataPos > layerClose)
      throw std::runtime_error("TMX: tile layer missing <data>");

    std::vector<TmxProperty> layerProps;
    parsePropertiesBlock(xml, openEnd + 1, dataPos, layerProps);
    TiledLayerMeta layerMeta = buildLayerMeta(layerTag, layerProps);

    size_t dataGt = xml.find('>', dataPos);
    std::string dataOpen = std::string(xml.substr(dataPos, dataGt - dataPos + 1));

    size_t dataClose = xml.find("</data>", dataGt);
    if (dataClose == std::string::npos)
      throw std::runtime_error("TMX: unclosed <data>");
    std::string dataBody =
        trim(std::string(xml.substr(dataGt + 1, dataClose - dataGt - 1)));

    int lw = parseIntAttr(layerTag, "width", mapW);
    int lh = parseIntAttr(layerTag, "height", mapH);

    terrain.AddLayer();
    terrain.tmxMeta.layerInfo.push_back(std::move(layerMeta));
    int layerIdx = static_cast<int>(terrain.layers.size()) - 1;
    tileLayerFilePos.push_back({layerXmlPos, layerIdx});

    TileBounds thisLayerBounds;

    if (dataBody.find("<chunk") != std::string::npos) {
      size_t cpos = 0;
      while ((cpos = dataBody.find("<chunk", cpos)) != std::string::npos) {
        size_t cgt = dataBody.find('>', cpos);
        if (cgt == std::string::npos)
          throw std::runtime_error("TMX: malformed <chunk>");
        std::string chunkTag = dataBody.substr(cpos, cgt - cpos + 1);
        int cx = parseIntAttr(chunkTag, "x", 0);
        int cy = parseIntAttr(chunkTag, "y", 0);
        int cw = parseIntAttr(chunkTag, "width", 16);
        int ch = parseIntAttr(chunkTag, "height", 16);
        size_t cClose = dataBody.find("</chunk>", cgt);
        if (cClose == std::string::npos)
          throw std::runtime_error("TMX: unclosed <chunk>");
        std::string chunkBody = trim(dataBody.substr(cgt + 1, cClose - (cgt + 1)));
        std::vector<uint32_t> gids = decodeChunkGids(chunkBody, cw, ch);
        for (int ty = 0; ty < ch; ++ty) {
          for (int tx = 0; tx < cw; ++tx) {
            uint32_t raw = gids[static_cast<size_t>(ty * cw + tx)];
            uint32_t gid = raw & 0x1FFFFFFFu;
            int store = (gid == 0) ? 0 : static_cast<int>(gid);
            int wtx = cx + tx;
            int wty = cy + ty;
            terrain.SetTile(layerIdx, wtx, wty, store);
            thisLayerBounds.add(wtx, wty);
          }
        }
        cpos = cClose + 8;
      }
    } else {
      if (lw <= 0 || lh <= 0)
        throw std::runtime_error("TMX: layer missing width/height for non-chunk data");
      std::vector<uint32_t> gids = decodeLayerDataGids(dataOpen, dataBody, lw, lh);
      for (int ty = 0; ty < lh; ++ty) {
        for (int tx = 0; tx < lw; ++tx) {
          uint32_t raw = gids[static_cast<size_t>(ty * lw + tx)];
          uint32_t gid = raw & 0x1FFFFFFFu;
          int store = (gid == 0) ? 0 : static_cast<int>(gid);
          terrain.SetTile(layerIdx, tx, ty, store);
          thisLayerBounds.add(tx, ty);
        }
      }
    }

    layerBounds.merge(thisLayerBounds);
    pos = layerClose + 8;
  }

  std::vector<std::pair<size_t, TiledImageLayerMeta>> imageParts;
  size_t ip = 0;
  while ((ip = xml.find("<imagelayer", ip)) != std::string::npos) {
    TiledImageLayerMeta im;
    size_t next = 0;
    parseOneImagelayer(path, xml, ip, im, next);
    imageParts.push_back({ip, std::move(im)});
    ip = next;
  }
  std::stable_sort(imageParts.begin(), imageParts.end(),
                   [](const auto &a, const auto &b) { return a.first < b.first; });
  for (auto &p : imageParts)
    terrain.tmxMeta.imageLayers.push_back(std::move(p.second));

  if (!imageParts.empty()) {
    struct DrawSort {
      size_t pos;
      TmxDrawLayerKind kind;
      int index;
    };
    std::vector<DrawSort> drawSort;
    for (const auto &te : tileLayerFilePos)
      drawSort.push_back({te.first, TmxDrawLayerKind::Tile, te.second});
    for (size_t ii = 0; ii < imageParts.size(); ++ii)
      drawSort.push_back({imageParts[ii].first, TmxDrawLayerKind::Image, static_cast<int>(ii)});
    std::stable_sort(drawSort.begin(), drawSort.end(),
                     [](const DrawSort &a, const DrawSort &b) { return a.pos < b.pos; });
    for (const DrawSort &d : drawSort)
      terrain.tmxMeta.drawLayerOrder.push_back({d.kind, d.index});
  }

  if (terrain.layers.empty() && terrain.tmxMeta.imageLayers.empty())
    throw std::runtime_error("TMX: no tile layers or image layers");

  if (mapInfinite) {
    if (!layerBounds.has)
      throw std::runtime_error("TMX: infinite map has no tile bounds");
    terrain.SetTmxLogicalExtent(layerBounds.maxTx - layerBounds.minTx,
                                layerBounds.maxTy - layerBounds.minTy);
    terrain.tmxMeta.boundsMinTx = layerBounds.minTx;
    terrain.tmxMeta.boundsMinTy = layerBounds.minTy;
    terrain.tmxMeta.boundsMaxTx = layerBounds.maxTx;
    terrain.tmxMeta.boundsMaxTy = layerBounds.maxTy;
  } else {
    terrain.SetTmxLogicalExtent(mapW, mapH);
    terrain.tmxMeta.boundsMinTx = 0;
    terrain.tmxMeta.boundsMinTy = 0;
    terrain.tmxMeta.boundsMaxTx = mapW;
    terrain.tmxMeta.boundsMaxTy = mapH;
  }

  parseObjectGroups(xml, terrain.tmxMeta.objectGroups);

  return terrain;
}

} // namespace criogenio
