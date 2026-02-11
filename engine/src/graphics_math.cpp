#include "graphics_types.h"

namespace criogenio {

Vec2 ScreenToWorld2D(Vec2 screenPos, const Camera2D& camera, float viewportW,
                     float viewportH) {
  float halfW = viewportW * 0.5f;
  float halfH = viewportH * 0.5f;
  float invZoom = 1.0f / (camera.zoom != 0 ? camera.zoom : 1.0f);
  Vec2 world;
  world.x = (screenPos.x - halfW - camera.offset.x) * invZoom + camera.target.x;
  world.y = (screenPos.y - halfH - camera.offset.y) * invZoom + camera.target.y;
  return world;
}

Vec2 WorldToScreen2D(Vec2 worldPos, const Camera2D& camera, float viewportW,
                    float viewportH) {
  float halfW = viewportW * 0.5f;
  float halfH = viewportH * 0.5f;
  Vec2 screen;
  screen.x = (worldPos.x - camera.target.x) * camera.zoom + camera.offset.x + halfW;
  screen.y = (worldPos.y - camera.target.y) * camera.zoom + camera.offset.y + halfH;
  return screen;
}

} // namespace criogenio
