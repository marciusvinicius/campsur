#include "subterra_player_json_io.h"
#include "animation_state.h"
#include "asset_manager.h"
#include "criogenio_io.h"
#include "json.hpp"
#include "resources.h"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace criogenio {
namespace fs = std::filesystem;

namespace {

static bool MapAnimName(const std::string &n, AnimState *out) {
  if (n == "idle") {
    *out = AnimState::IDLE;
    return true;
  }
  if (n == "walk") {
    *out = AnimState::WALKING;
    return true;
  }
  if (n == "run") {
    *out = AnimState::RUNNING;
    return true;
  }
  if (n == "jump") {
    *out = AnimState::JUMPING;
    return true;
  }
  if (n == "attack") {
    *out = AnimState::ATTACKING;
    return true;
  }
  return false;
}

static const char *StateClipPrefix(AnimState s) {
  switch (s) {
  case AnimState::IDLE:
    return "idle";
  case AnimState::WALKING:
    return "walk";
  case AnimState::RUNNING:
    return "run";
  case AnimState::JUMPING:
    return "jump";
  case AnimState::ATTACKING:
    return "attack";
  default:
    return "idle";
  }
}

static const AnimationClip *FindClip(const AnimationDef &def, AnimState st, Direction d) {
  for (const auto &c : def.clips) {
    if (c.state == st && c.direction == d)
      return &c;
  }
  return nullptr;
}

static int DirIndex(const std::vector<std::string> &order, Direction d) {
  const std::string key = direction_to_string(d);
  for (size_t i = 0; i < order.size(); ++i) {
    if (order[i] == key)
      return static_cast<int>(i);
  }
  return 0;
}

static bool NearlyEq(float a, float b, float eps = 0.51f) { return std::fabs(a - b) < eps; }

} // namespace

AssetId LoadSubterraPlayerAnimationJson(const std::string &jsonPath, int *outFrameW,
                                        int *outFrameH, std::string *errOut) {
  auto fail = [&](const char *msg) -> AssetId {
    if (errOut)
      *errOut = msg;
    return INVALID_ASSET_ID;
  };

  std::ifstream f(jsonPath);
  if (!f)
    return fail("could not open json file");

  nlohmann::json j;
  try {
    f >> j;
  } catch (...) {
    return fail("invalid json");
  }

  fs::path jsonP = fs::path(jsonPath).lexically_normal();
  fs::path projectRoot = jsonP.parent_path().parent_path().parent_path();

  std::string texRel = j.value("male_sheet", std::string());
  if (texRel.empty())
    texRel = j.value("female_sheet", std::string());
  if (texRel.empty())
    texRel = j.value("texture", std::string());
  if (texRel.empty())
    return fail("missing sheet path (male_sheet, female_sheet, or texture)");

  fs::path texPath = projectRoot / texRel;
  std::string texAbs = texPath.lexically_normal().string();

  const int frameW = j.value("frame_width", 64);
  const int frameH = j.value("frame_height", 64);
  if (outFrameW)
    *outFrameW = frameW;
  if (outFrameH)
    *outFrameH = frameH;

  std::vector<std::string> order = {"up", "left", "down", "right"};
  if (j.contains("direction_order") && j["direction_order"].is_array()) {
    order.clear();
    for (const auto &el : j["direction_order"]) {
      if (el.is_string())
        order.push_back(el.get<std::string>());
    }
  }
  std::unordered_map<std::string, int> dirRowPlus;
  for (size_t i = 0; i < order.size(); ++i)
    dirRowPlus[order[i]] = static_cast<int>(i);

  auto dirIndex = [&](Direction d) -> int {
    const std::string key = direction_to_string(d);
    auto it = dirRowPlus.find(key);
    if (it != dirRowPlus.end())
      return it->second;
    return 0;
  };

  if (!j.contains("animations") || !j["animations"].is_array())
    return fail("missing animations array");

  AnimationDatabase &db = AnimationDatabase::instance();
  AssetId animId = db.createAnimation(texAbs);

  for (const auto &animEl : j["animations"]) {
    if (!animEl.is_object())
      continue;
    const std::string name = animEl.value("name", std::string());
    AnimState state{};
    if (!MapAnimName(name, &state))
      continue;

    const int startRow = animEl.value("start_row", 0);
    const int frameCount = animEl.value("frame_count", 1);
    float fps = 10.f;
    if (animEl.contains("fps")) {
      if (animEl["fps"].is_number_float())
        fps = animEl["fps"].get<float>();
      else if (animEl["fps"].is_number_integer())
        fps = static_cast<float>(animEl["fps"].get<int>());
    }
    if (fps <= 0.001f)
      fps = 10.f;
    const float frameSpeed = 1.f / fps;

    const char *prefix = StateClipPrefix(state);

    for (int di = 0; di < 4; ++di) {
      const auto d = static_cast<Direction>(di);
      const int row = startRow + dirIndex(d);
      AnimationClip clip;
      clip.name = std::string(prefix) + "_" + direction_to_string(d);
      clip.frameSpeed = frameSpeed;
      clip.state = state;
      clip.direction = d;
      for (int fi = 0; fi < frameCount; ++fi) {
        AnimationFrame fr;
        fr.rect = {static_cast<float>(fi * frameW), static_cast<float>(row * frameH),
                   static_cast<float>(frameW), static_cast<float>(frameH)};
        clip.frames.push_back(fr);
      }
      db.addClip(animId, clip);
    }
  }

  auto texture = AssetManager::instance().load<TextureResource>(texAbs);
  if (!texture)
    ENGINE_LOG(LOG_WARNING, "LoadSubterraPlayerAnimationJson: texture load failed: %s",
               texAbs.c_str());

  return animId;
}

bool SaveSubterraPlayerAnimationJson(AssetId animId, const std::string &jsonPath,
                                     std::string *errOut) {
  auto fail = [&](const std::string &msg) {
    if (errOut)
      *errOut = msg;
    return false;
  };

  const AnimationDef *def = AnimationDatabase::instance().getAnimation(animId);
  if (!def)
    return fail("invalid animation id");
  if (def->clips.empty())
    return fail("animation has no clips");

  const float fw = def->clips[0].frames[0].rect.width;
  const float fh = def->clips[0].frames[0].rect.height;
  if (fw < 1.f || fh < 1.f)
    return fail("invalid frame size");

  for (const auto &clip : def->clips) {
    for (const auto &fr : clip.frames) {
      if (!NearlyEq(fr.rect.width, fw) || !NearlyEq(fr.rect.height, fh))
        return fail("all frames must share the same width/height for Subterra export");
    }
  }

  const std::vector<std::string> order = {"up", "left", "down", "right"};

  struct ExportRow {
    std::string jsonName;
    int start_row = 0;
    int frame_count = 0;
    float fps = 10.f;
    bool loop = true;
  };
  std::vector<ExportRow> rows;

  const AnimState kStates[] = {AnimState::IDLE, AnimState::WALKING, AnimState::RUNNING,
                               AnimState::JUMPING, AnimState::ATTACKING};
  const char *kJsonNames[] = {"idle", "walk", "run", "jump", "attack"};

  for (size_t si = 0; si < sizeof(kStates) / sizeof(kStates[0]); ++si) {
    const AnimState st = kStates[si];
    int startRow = -1;
    int frameCount = -1;
    float frameSpeed = -1.f;
    bool stateOk = true;

    for (int di = 0; di < 4; ++di) {
      const Direction d = static_cast<Direction>(di);
      const AnimationClip *c = FindClip(*def, st, d);
      if (!c || c->frames.empty()) {
        stateOk = false;
        break;
      }

      const int row = static_cast<int>(std::lround(c->frames[0].rect.y / fh));
      const int candStart = row - DirIndex(order, d);
      if (startRow < 0)
        startRow = candStart;
      else if (std::abs(candStart - startRow) > 0)
        return fail("rows for " + std::string(kJsonNames[si]) +
                    " do not match direction_order spacing (check clip.state/direction)");

      if (frameCount < 0)
        frameCount = static_cast<int>(c->frames.size());
      else if (static_cast<int>(c->frames.size()) != frameCount)
        return fail("frame_count mismatch between directions for " +
                    std::string(kJsonNames[si]));

      if (frameSpeed < 0.f)
        frameSpeed = c->frameSpeed;
      else if (!NearlyEq(c->frameSpeed, frameSpeed))
        return fail("frameSpeed mismatch between directions for " + std::string(kJsonNames[si]));

      for (size_t fi = 0; fi < c->frames.size(); ++fi) {
        const auto &fr = c->frames[fi];
        if (!NearlyEq(fr.rect.x, static_cast<float>(fi) * fw))
          return fail("frames must be a horizontal strip (x = i * frame_width) for Subterra "
                      "export");
        if (!NearlyEq(fr.rect.y, static_cast<float>(row) * fh))
          return fail("frame y does not match expected row for Subterra export");
      }
    }

    if (!stateOk)
      continue;

    ExportRow er;
    er.jsonName = kJsonNames[si];
    er.start_row = startRow;
    er.frame_count = frameCount;
    if (frameSpeed > 1e-6f)
      er.fps = 1.f / frameSpeed;
    else
      er.fps = 10.f;
    er.loop = !(st == AnimState::ATTACKING || st == AnimState::JUMPING);
    rows.push_back(er);
  }

  if (rows.empty())
    return fail("no exportable states (need idle/walk/run/jump/attack with all four directions "
                "and row-strip layout)");

  fs::path jsonP = fs::path(jsonPath).lexically_normal();
  fs::path projectRoot = jsonP.parent_path().parent_path().parent_path();

  std::string texRel;
  {
    std::error_code ec;
    fs::path texAbs = fs::weakly_canonical(def->texturePath, ec);
    if (ec)
      texAbs = fs::path(def->texturePath);
    fs::path rel = fs::relative(texAbs, projectRoot, ec);
    if (!ec)
      texRel = rel.generic_string();
    else
      texRel = def->texturePath;
  }

  nlohmann::json j;
  j["male_sheet"] = texRel;
  j["frame_width"] = static_cast<int>(std::lround(fw));
  j["frame_height"] = static_cast<int>(std::lround(fh));
  j["direction_count"] = 4;
  j["direction_order"] = order;
  j["animations"] = nlohmann::json::array();
  for (const auto &er : rows) {
    nlohmann::json a;
    a["name"] = er.jsonName;
    a["start_row"] = er.start_row;
    a["frame_count"] = er.frame_count;
    a["fps"] = er.fps;
    a["loop"] = er.loop;
    j["animations"].push_back(a);
  }

  std::ofstream out(jsonPath);
  if (!out)
    return fail("could not open output path for write");
  out << j.dump(2);
  return true;
}

static AssetId loadSubterraAssetPlaceholderJson(const std::string &jsonPath,
                                                const nlohmann::json &j, int *outFrameW,
                                                int *outFrameH, std::string *errOut) {
  auto fail = [&](const char *msg) -> AssetId {
    if (errOut)
      *errOut = msg;
    return INVALID_ASSET_ID;
  };

  fs::path jsonP = fs::path(jsonPath).lexically_normal();
  fs::path projectRoot = jsonP.parent_path().parent_path().parent_path();
  std::string texRel = j.value("texture", std::string());
  if (texRel.empty())
    return fail("asset animation: missing texture");

  std::string texAbs = (projectRoot / texRel).lexically_normal().string();

  int frameW = 16;
  if (j.contains("frame_width") && j["frame_width"].is_number_integer())
    frameW = j["frame_width"].get<int>();
  else if (j.contains("tile_width") && j["tile_width"].is_number_integer())
    frameW = j["tile_width"].get<int>();
  int frameH = 16;
  if (j.contains("frame_height") && j["frame_height"].is_number_integer())
    frameH = j["frame_height"].get<int>();
  else if (j.contains("tile_height") && j["tile_height"].is_number_integer())
    frameH = j["tile_height"].get<int>();
  if (outFrameW)
    *outFrameW = frameW;
  if (outFrameH)
    *outFrameH = frameH;

  if (!j.contains("frames") || !j["frames"].is_array() || j["frames"].empty())
    return fail("asset animation: missing or empty top-level frames[]");

  int maxTileIndex = 0;
  for (const auto &cue : j["frames"]) {
    if (!cue.is_object() || !cue.contains("frames") || !cue["frames"].is_array())
      continue;
    for (const auto &idxEl : cue["frames"]) {
      if (idxEl.is_number_integer()) {
        int v = idxEl.get<int>();
        if (v > maxTileIndex)
          maxTileIndex = v;
      } else if (idxEl.is_number_unsigned()) {
        int v = static_cast<int>(idxEl.get<unsigned>());
        if (v > maxTileIndex)
          maxTileIndex = v;
      }
    }
  }
  const int cols = std::max(1, maxTileIndex + 1);

  AnimationDatabase &db = AnimationDatabase::instance();
  AssetId animId = db.createAnimation(texAbs);

  for (const auto &cue : j["frames"]) {
    if (!cue.is_object())
      continue;
    std::string clipName = cue.value("name", std::string());
    if (clipName.empty())
      clipName = "clip";

    int durationMs = 100;
    if (cue.contains("duration")) {
      if (cue["duration"].is_number_integer())
        durationMs = cue["duration"].get<int>();
      else if (cue["duration"].is_number_unsigned())
        durationMs = static_cast<int>(cue["duration"].get<unsigned>());
      else if (cue["duration"].is_number_float())
        durationMs = static_cast<int>(cue["duration"].get<double>());
    }
    const float frameSpeed = static_cast<float>(std::max(1, durationMs)) / 1000.f;

    AnimationClip clip;
    clip.name = std::move(clipName);
    clip.frameSpeed = frameSpeed;
    clip.state = AnimState::IDLE;
    clip.direction = Direction::RIGHT;

    if (!cue.contains("frames") || !cue["frames"].is_array())
      continue;
    for (const auto &idxEl : cue["frames"]) {
      int tile = 0;
      if (idxEl.is_number_integer())
        tile = idxEl.get<int>();
      else if (idxEl.is_number_unsigned())
        tile = static_cast<int>(idxEl.get<unsigned>());
      else
        continue;
      const int col = tile % cols;
      const int row = tile / cols;
      AnimationFrame fr;
      fr.rect = {static_cast<float>(col * frameW), static_cast<float>(row * frameH),
                 static_cast<float>(frameW), static_cast<float>(frameH)};
      clip.frames.push_back(fr);
    }
    if (!clip.frames.empty())
      db.addClip(animId, clip);
  }

  auto texture = AssetManager::instance().load<TextureResource>(texAbs);
  if (!texture)
    ENGINE_LOG(LOG_WARNING, "loadSubterraAssetPlaceholderJson: texture load failed: %s",
               texAbs.c_str());

  const AnimationDef *def = db.getAnimation(animId);
  if (!def || def->clips.empty())
    return fail("asset animation: no clips could be built from frames[]");

  return animId;
}

AssetId LoadSubterraGuildAnimationJson(const std::string &jsonPath, int *outFrameW,
                                       int *outFrameH, std::string *errOut) {
  auto fail = [&](const char *msg) -> AssetId {
    if (errOut)
      *errOut = msg;
    return INVALID_ASSET_ID;
  };

  std::ifstream f(jsonPath);
  if (!f)
    return fail("could not open json file");

  nlohmann::json j;
  try {
    f >> j;
  } catch (...) {
    return fail("invalid json");
  }

  if (j.contains("texturePath") && j["texturePath"].is_string() && j.contains("clips") &&
      j["clips"].is_array()) {
    return LoadAnimationFromFile(jsonPath);
  }

  bool hasStripAnimations = false;
  if (j.contains("animations") && j["animations"].is_array()) {
    for (const auto &el : j["animations"]) {
      if (el.is_object() && el.contains("start_row")) {
        hasStripAnimations = true;
        break;
      }
    }
  }
  if (hasStripAnimations)
    return LoadSubterraPlayerAnimationJson(jsonPath, outFrameW, outFrameH, errOut);

  const bool assetTopLevelFrames =
      j.contains("texture") && j["texture"].is_string() && j.contains("frames") &&
      j["frames"].is_array() && !j["frames"].empty() && !j.contains("animations");
  if (assetTopLevelFrames) {
    const auto &first = j["frames"][0];
    if (first.is_object() &&
        (first.contains("duration") || first.contains("frames"))) {
      return loadSubterraAssetPlaceholderJson(jsonPath, j, outFrameW, outFrameH, errOut);
    }
  }

  return fail("unrecognized JSON (use engine clip export, Subterra strip + start_row, or "
              "data/animations asset with texture + frames[])");
}

} // namespace criogenio
