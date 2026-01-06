// Minimal AssetManager implementation
#include "../include/asset_manager.h"

#include <utility>

namespace criogenio {

AssetManager &AssetManager::instance() {
  static AssetManager inst;
  return inst;
}

void AssetManager::unload(const std::string &path) {
  std::lock_guard<std::mutex> lk(mutex_);
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (it->first.path == path)
      it = cache_.erase(it);
    else
      ++it;
  }
}

void AssetManager::registerHotReloadCallback(const std::string &path,
                                             HotReloadCallback cb) {
  std::lock_guard<std::mutex> lk(mutex_);
  hot_reload_callbacks_[path].push_back(std::move(cb));
}

void AssetManager::triggerHotReload(const std::string &path) {
  // Reload resources of the given path using their registered loaders.
  std::vector<Key> keysToReload;
  {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto const &p : cache_)
      if (p.first.path == path)
        keysToReload.push_back(p.first);
  }

  for (auto const &k : keysToReload) {
    std::function<std::shared_ptr<Resource>(const std::string &)> loader;
    {
      std::lock_guard<std::mutex> lk(mutex_);
      auto it = loaders_.find(k.type);
      if (it == loaders_.end())
        continue;
      loader = it->second;
    }
    auto newRes = loader(path);
    if (newRes) {
      std::lock_guard<std::mutex> lk(mutex_);
      cache_[k] = newRes;
    }
  }

  // invoke callbacks
  std::vector<HotReloadCallback> cbs;
  {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = hot_reload_callbacks_.find(path);
    if (it != hot_reload_callbacks_.end())
      cbs = it->second;
  }
  for (auto &cb : cbs)
    cb(path);
}

} // namespace criogenio
