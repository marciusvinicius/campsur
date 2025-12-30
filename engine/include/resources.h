#pragma once

#include "asset_manager.h"
#include "raylib.h"
#include <memory>
#include <string>

namespace criogenio {

struct TextureResource : public Resource {
  Texture2D texture{};
  std::string path;
  TextureResource() = default;
  TextureResource(const std::string &p, const Texture2D &t)
      : texture(t), path(p) {}
  ~TextureResource() override {
    if (texture.id)
      UnloadTexture(texture);
  }
};

struct ModelResource : public Resource {
  Model model{};
  std::string path;
  ModelResource() = default;
  ModelResource(const std::string &p, const Model &m) : model(m), path(p) {}
  ~ModelResource() override {
    // UnloadModel is available in raylib; leave for future
  }
};

} // namespace criogenio
