#pragma once

#include "graphics_types.h"
#include <functional>
#include <string>

namespace criogenio {

class Renderer {
public:
  Renderer(int width, int height, const std::string& title);
  ~Renderer();

  /** False if SDL window/renderer creation failed; Run() will exit immediately otherwise. */
  bool IsValid() const;
  /** If IsValid() is false, returns the SDL error string from init failure. */
  const char* GetInitError() const;

  void BeginFrame();
  void EndFrame();

  void SetViewport(int width, int height);
  void BeginCamera2D(const Camera2D& camera);
  void EndCamera2D();

  void DrawTexture(TextureHandle texture, float x, float y);
  void DrawRect(float x, float y, float w, float h, Color color);
  void DrawRectOutline(float x, float y, float w, float h, Color color);
  void DrawCircle(float x, float y, float r, Color color);
  void DrawTextString(const std::string& text, int x, int y, int size,
                      Color color);
  /** SDL_RenderDebugText: tiny ASCII debug font, screen space (call outside world camera). */
  void DrawDebugText(float x, float y, const char* utf8);
  void DrawTexturePro(TextureHandle texture, Rect source, Rect dest, Vec2 origin,
                     float rotation, Color tint,
                     TextureBlendMode blend = TextureBlendMode::Alpha);
  /**
   * Same as DrawTexturePro but applies horizontal/vertical flip via SDL.
   * Always uses SDL_RenderTextureRotated under the hood, even when rotation
   * is zero, because SDL3's untextured RenderTexture path doesn't accept a
   * flip argument.
   */
  void DrawTextureProFlipped(TextureHandle texture, Rect source, Rect dest,
                             Vec2 origin, float rotation, Color tint,
                             bool flipH, bool flipV,
                             TextureBlendMode blend = TextureBlendMode::Alpha);
  void DrawTextureRec(TextureHandle texture, Rect source, Vec2 position,
                      Color tint,
                      TextureBlendMode blend = TextureBlendMode::Alpha);
  void DrawLine(float x1, float y1, float x2, float y2, Color color);
  void DrawGrid(int slices, float spacing);

  /** Blend mode for `DrawRect` / `DrawFilledCircleScreen` (not textures). Default is alpha. */
  void SetRenderDrawBlendMode(TextureBlendMode blend);
  /** Axis-aligned rect in raw window pixels; ignores the 2D camera (for post-world fullscreen passes). */
  void DrawRectScreen(float x, float y, float w, float h, Color color);
  /** Filled disk in window pixel space; ignores the 2D camera. */
  void DrawFilledCircleScreen(float centerX, float centerY, float radius, Color color);

  bool WindowShouldClose() const;
  /**
   * Process SDL events. If consumeFirst is set and returns true for an event, default handling
   * (quit on Escape, window resize bookkeeping) is skipped for that event.
   */
  void ProcessEvents(std::function<bool(const void* sdlEvent)>* consumeFirst = nullptr);

  int GetViewportWidth() const;
  int GetViewportHeight() const;

  TextureHandle LoadTexture(const std::string& path);
  void UnloadTexture(TextureHandle* handle);

  // Render target (for editor scene view). CreateRenderTarget -> SetRenderTarget -> draw -> UnsetRenderTarget.
  TextureHandle CreateRenderTarget(int width, int height);
  void SetRenderTarget(TextureHandle target);
  void UnsetRenderTarget();
  void DestroyRenderTarget(TextureHandle* target);

  Vec2 GetMousePosition() const;

  // For ImGui/editor: get native window and renderer. Opaque to API (SDL_Window* / SDL_Renderer*).
  void* GetWindowHandle() const;
  void* GetRendererHandle() const;
};

} // namespace criogenio
