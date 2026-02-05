#pragma once

namespace criogenio {

struct Vec2 {
  float x = 0;
  float y = 0;
};

struct Rect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
};

struct Color {
  unsigned char r = 255;
  unsigned char g = 255;
  unsigned char b = 255;
  unsigned char a = 255;
};

inline bool operator==(const Color& a, const Color& b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

struct Camera2D {
  Vec2 offset{0, 0};
  Vec2 target{0, 0};
  float rotation = 0;
  float zoom = 1.0f;
};

// Opaque handle for a loaded texture (backend-specific internally)
struct TextureHandle {
  void* opaque = nullptr;
  int width = 0;
  int height = 0;
  bool valid() const { return opaque != nullptr; }
};

// Common colors
namespace Colors {
constexpr Color White{255, 255, 255, 255};
constexpr Color Black{0, 0, 0, 255};
constexpr Color Red{255, 0, 0, 255};
constexpr Color Green{0, 255, 0, 255};
constexpr Color Blue{0, 0, 255, 255};
constexpr Color Yellow{255, 255, 0, 255};
constexpr Color Gray{128, 128, 128, 255};
constexpr Color DarkGray{64, 64, 64, 255};
} // namespace Colors

// Screen space <-> world space (2D camera). viewportW/H = size of the viewport.
Vec2 ScreenToWorld2D(Vec2 screenPos, const Camera2D& camera, float viewportW,
                     float viewportH);
Vec2 WorldToScreen2D(Vec2 worldPos, const Camera2D& camera, float viewportW,
                    float viewportH);

// Point-in-rect test (for UI / picking).
inline bool PointInRect(Vec2 p, Rect r) {
  return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}

} // namespace criogenio
