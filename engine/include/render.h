#pragma once

#include "graphics_types.h"
#include <functional>
#include <string>

namespace criogenio {

class Renderer {
public:
  Renderer(int width, int height, const std::string& title);
  ~Renderer();

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
  void DrawTexturePro(TextureHandle texture, Rect source, Rect dest, Vec2 origin,
                     float rotation, Color tint);
  void DrawTextureRec(TextureHandle texture, Rect source, Vec2 position,
                      Color tint);
  void DrawLine(float x1, float y1, float x2, float y2, Color color);
  void DrawGrid(int slices, float spacing);

  bool WindowShouldClose() const;
  // Call each frame. If onEvent is set, it is called for each SDL event (so e.g. ImGui can process it).
  void ProcessEvents(std::function<void(const void* sdlEvent)>* onEvent = nullptr);

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
