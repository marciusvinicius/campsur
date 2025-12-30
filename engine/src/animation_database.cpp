#include "animation_database.h"
#include <string>

namespace criogenio {

AnimationDatabase &AnimationDatabase::instance() {
  static AnimationDatabase db;
  return db;
}

AssetId AnimationDatabase::createAnimation(const std::string &texturePath) {
  AssetId id = nextId_++;
  animations_[id] = {id, texturePath, {}};
  refcounts_[id] = 0;
  return id;
}

void AnimationDatabase::addClip(AssetId animId, const AnimationClip &clip) {
  auto it = animations_.find(animId);
  if (it != animations_.end()) {
    it->second.clips.push_back(clip);
  }
}

void AnimationDatabase::addReference(AssetId animId) {
  auto it = refcounts_.find(animId);
  if (it == refcounts_.end())
    refcounts_[animId] = 1;
  else
    ++(it->second);
}

void AnimationDatabase::removeReference(AssetId animId) {
  auto it = refcounts_.find(animId);
  if (it == refcounts_.end())
    return;
  --(it->second);
  if (it->second <= 0)
    refcounts_.erase(it);
}

int AnimationDatabase::getRefCount(AssetId animId) const {
  auto it = refcounts_.find(animId);
  return (it == refcounts_.end()) ? 0 : it->second;
}

const AnimationDef *AnimationDatabase::getAnimation(AssetId animId) const {
  auto it = animations_.find(animId);
  return (it != animations_.end()) ? &it->second : nullptr;
}

const AnimationClip *
AnimationDatabase::getClip(AssetId animId, const std::string &clipName) const {
  const auto *animDef = getAnimation(animId);
  if (!animDef)
    return nullptr;

  for (const auto &clip : animDef->clips) {
    if (clip.name == clipName)
      return &clip;
  }
  return nullptr;
}

void AnimationDatabase::clear() {
  animations_.clear();
  nextId_ = INVALID_ASSET_ID + 1;
}

std::string AnimationDatabase::setTexturePath(AssetId animId,
                                              const std::string &newPath) {
  auto it = animations_.find(animId);
  if (it == animations_.end())
    return std::string();
  std::string old = it->second.texturePath;
  it->second.texturePath = newPath;
  return old;
}

AssetId AnimationDatabase::cloneAnimation(AssetId sourceId) {
  auto it = animations_.find(sourceId);
  if (it == animations_.end())
    return INVALID_ASSET_ID;

  AssetId newId = nextId_++;
  AnimationDef def = it->second; // copy
  def.id = newId;
  animations_.emplace(newId, std::move(def));
  // copy refcount as 0 since new asset is independent until assigned
  refcounts_[newId] = 0;
  return newId;
}

} // namespace criogenio
