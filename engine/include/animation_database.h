#pragma once

#include "raylib.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace criogenio {

// Simple asset ID type
using AssetId = uint32_t;
constexpr AssetId INVALID_ASSET_ID = 0;

struct AnimationFrame {
  Rectangle rect;
};

struct AnimationClip {
  std::string name;
  std::vector<AnimationFrame> frames;
  float frameSpeed = 0.2f;
};

struct AnimationDef {
  AssetId id;
  std::string texturePath;
  std::vector<AnimationClip> clips;
};

class AnimationDatabase {
public:
  static AnimationDatabase &instance();

  AssetId createAnimation(const std::string &texturePath);

  void addClip(AssetId animId, const AnimationClip &clip);

  const AnimationDef *getAnimation(AssetId animId) const;
  const AnimationClip *getClip(AssetId animId,
                               const std::string &clipName) const;

  // Update the texture path for an existing animation. Returns the previous
  // texture path (empty if none or animId not found).
  std::string setTexturePath(AssetId animId, const std::string &newPath);
  // Create a copy of an existing animation (clips + texturePath) and return
  // the new AssetId. Returns INVALID_ASSET_ID if source not found.
  AssetId cloneAnimation(AssetId sourceId);

  void clear();

  // Reference counting helpers for usage tracking
  void addReference(AssetId animId);
  void removeReference(AssetId animId);
  int getRefCount(AssetId animId) const;

private:
  AnimationDatabase() : nextId_(INVALID_ASSET_ID + 1) {}
  ~AnimationDatabase() {
    clear(); // Clean up on destruction
  }

  AssetId nextId_;
  std::unordered_map<AssetId, AnimationDef> animations_;
  std::unordered_map<AssetId, int> refcounts_;
};

} // namespace criogenio
