#include "player_anim.h"
#include "animation_state.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <stdexcept>
#include <unordered_map>

namespace subterra {
namespace fs = std::filesystem;

static bool MapAnimName(const std::string &n, criogenio::AnimState *out) {
  using criogenio::AnimState;
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
  return false;
}

static const char *StateClipPrefix(criogenio::AnimState s) {
  using criogenio::AnimState;
  switch (s) {
  case AnimState::IDLE:
    return "idle";
  case AnimState::WALKING:
    return "walk";
  case AnimState::RUNNING:
    return "run";
  default:
    return "idle";
  }
}

criogenio::AssetId LoadPlayerAnimationDatabaseFromJson(const std::string &jsonPath,
                                                       PlayerAnimConfig *outCfg) {
  std::ifstream f(jsonPath);
  if (!f)
    return criogenio::INVALID_ASSET_ID;

  nlohmann::json j;
  try {
    f >> j;
  } catch (...) {
    return criogenio::INVALID_ASSET_ID;
  }

  fs::path jsonP = fs::path(jsonPath).lexically_normal();
  fs::path projectRoot = jsonP.parent_path().parent_path().parent_path();

  std::string texRel = j.value("male_sheet", std::string());
  if (texRel.empty())
    texRel = j.value("female_sheet", std::string());
  if (texRel.empty())
    return criogenio::INVALID_ASSET_ID;

  fs::path texPath = projectRoot / texRel;
  std::string texAbs = texPath.lexically_normal().string();

  const int frameW = j.value("frame_width", 64);
  const int frameH = j.value("frame_height", 64);
  if (outCfg) {
    outCfg->frameW = frameW;
    outCfg->frameH = frameH;
  }

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

  auto dirIndex = [&](criogenio::Direction d) -> int {
    std::string key = criogenio::direction_to_string(d);
    auto it = dirRowPlus.find(key);
    if (it != dirRowPlus.end())
      return it->second;
    return 0;
  };

  if (!j.contains("animations") || !j["animations"].is_array())
    return criogenio::INVALID_ASSET_ID;

  criogenio::AnimationDatabase &db = criogenio::AnimationDatabase::instance();
  criogenio::AssetId animId = db.createAnimation(texAbs);

  for (const auto &animEl : j["animations"]) {
    if (!animEl.is_object())
      continue;
    std::string name = animEl.value("name", std::string());
    criogenio::AnimState state{};
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
      const auto d = static_cast<criogenio::Direction>(di);
      const int row = startRow + dirIndex(d);
      criogenio::AnimationClip clip;
      clip.name = std::string(prefix) + "_" + criogenio::direction_to_string(d);
      clip.frameSpeed = frameSpeed;
      clip.state = state;
      clip.direction = d;
      for (int fi = 0; fi < frameCount; ++fi) {
        criogenio::AnimationFrame fr;
        fr.rect = {static_cast<float>(fi * frameW), static_cast<float>(row * frameH),
                   static_cast<float>(frameW), static_cast<float>(frameH)};
        clip.frames.push_back(fr);
      }
      db.addClip(animId, clip);
    }
  }

  return animId;
}

} // namespace subterra
