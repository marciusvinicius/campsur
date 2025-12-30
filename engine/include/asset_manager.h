#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace criogenio {

struct Resource {
  virtual ~Resource() = default;
};

class AssetManager {
public:
  using HotReloadCallback = std::function<void(const std::string &)>;

  static AssetManager &instance();

  template <typename T>
  void registerLoader(
      std::function<std::shared_ptr<T>(const std::string &)> loader) {
    std::lock_guard<std::mutex> lk(mutex_);
    loaders_[std::type_index(typeid(T))] =
        [loader](const std::string &p) -> std::shared_ptr<Resource> {
      return std::static_pointer_cast<Resource>(loader(p));
    };
  }

  template <typename T> std::shared_ptr<T> load(const std::string &path) {
    Key key{std::type_index(typeid(T)), path};
    {
      std::lock_guard<std::mutex> lk(mutex_);
      auto it = cache_.find(key);
      if (it != cache_.end())
        return std::static_pointer_cast<T>(it->second);
    }

    std::function<std::shared_ptr<Resource>(const std::string &)> loader;
    {
      std::lock_guard<std::mutex> lk(mutex_);
      auto it = loaders_.find(std::type_index(typeid(T)));
      if (it == loaders_.end())
        return nullptr;
      loader = it->second;
    }

    auto res = loader(path);
    if (res) {
      std::lock_guard<std::mutex> lk(mutex_);
      cache_.emplace(key, res);
    }
    return std::static_pointer_cast<T>(res);
  }

  template <typename T>
  void asyncLoad(const std::string &path,
                 std::function<void(std::shared_ptr<T>)> callback) {
    std::thread([this, path, callback]() mutable {
      auto r = load<T>(path);
      if (callback)
        callback(r);
    }).detach();
  }

  void unload(const std::string &path);

  void registerHotReloadCallback(const std::string &path, HotReloadCallback cb);
  void triggerHotReload(const std::string &path);

private:
  AssetManager() = default;
  ~AssetManager() = default;

  struct Key {
    std::type_index type{typeid(void)};
    std::string path;
    bool operator==(Key const &o) const {
      return type == o.type && path == o.path;
    }
  };

  struct KeyHash {
    size_t operator()(Key const &k) const noexcept {
      return k.type.hash_code() ^ (std::hash<std::string>()(k.path) << 1);
    }
  };

  std::mutex mutex_;
  std::unordered_map<Key, std::shared_ptr<Resource>, KeyHash> cache_;
  std::unordered_map<std::type_index, std::function<std::shared_ptr<Resource>(
                                          const std::string &)>>
      loaders_;
  std::unordered_map<std::string, std::vector<HotReloadCallback>>
      hot_reload_callbacks_;
};

} // namespace criogenio
