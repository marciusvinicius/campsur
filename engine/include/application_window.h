#pragma once

#include <cstdint>
#include <string>

namespace criogenio {

/// Bitmask for `WindowConfig::features`.
enum class WindowFeature : uint32_t {
  OpenGL = 1u << 0,
  Resizable = 1u << 1,
  HighPixelDensity = 1u << 2,
};

struct WindowConfig {
  int width = 1280;
  int height = 720;
  std::string title;
  /// Combine with `|` using underlying values, e.g.
  /// `(uint32_t)WindowFeature::OpenGL | (uint32_t)WindowFeature::Resizable`.
  uint32_t features = static_cast<uint32_t>(WindowFeature::Resizable);
};

/// SDL-backed window and event pump. Games should use this instead of including SDL directly.
class ApplicationWindow {
public:
  /// Call before creating a window when `WindowFeature::OpenGL` is used.
  static void PrepareOpenGLAttributes();

  explicit ApplicationWindow(const WindowConfig& config);
  ~ApplicationWindow();

  ApplicationWindow(const ApplicationWindow&) = delete;
  ApplicationWindow& operator=(const ApplicationWindow&) = delete;

  bool IsValid() const;
  const char* GetInitError() const;

  void SetRelativeMouseMode(bool enabled);

  /// Pump SDL events; call once per frame before `Input::` queries.
  void PollEvents();

  bool QuitRequested() const { return quitRequested_; }

  /// If the window was resized since the last `PollEvents`, writes size and returns true.
  bool ConsumeResize(int& outWidth, int& outHeight);

  void GetSize(int& outWidth, int& outHeight) const;

  /// Backbuffer size in pixels (use for OpenGL `glViewport` / projection when using HiDPI windows).
  void GetDrawableSize(int& outWidth, int& outHeight) const;

  /// Relative motion since last poll (use with `SetRelativeMouseMode(true)`).
  void GetRelativeMouse(float& outDx, float& outDy) const;

  /// Opaque `SDL_Window*` for engine subsystems (e.g. `GlBoxRenderer`). Do not cast in game code.
  void* GetPlatformWindowHandle() const;

private:
  struct Impl;
  Impl* impl_ = nullptr;
  bool quitRequested_ = false;
  std::string initError_;
};

} // namespace criogenio
