#pragma once

#include "asset_manager.h"
#include "graphics_types.h"
#include "render.h"
#include <memory>
#include <string>

namespace criogenio {

struct TextureResource : public Resource {
  TextureHandle texture{};
  std::string path;
  Renderer* renderer = nullptr; // for UnloadTexture in dtor

  TextureResource() = default;
  TextureResource(const std::string& p, TextureHandle t, Renderer* r)
      : texture(t), path(p), renderer(r) {}
  ~TextureResource() override {
    if (renderer && texture.valid()) {
      renderer->UnloadTexture(&texture);
    }
  }
};

struct ModelResource : public Resource {
  void* modelOpaque = nullptr; // placeholder for future 3D
  std::string path;
  ModelResource() = default;
  ModelResource(const std::string& p, void* m) : modelOpaque(m), path(p) {}
  ~ModelResource() override {}
};

} // namespace criogenio
