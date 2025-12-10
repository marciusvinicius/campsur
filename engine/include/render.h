#pragma once

#include "raylib.h"
#include <string>

namespace criogenio {

class Renderer {
public:
  Renderer(int width, int height, const std::string &title);
  ~Renderer();

  void BeginFrame();
  void EndFrame();

  void DrawTexture(::Texture2D texture, float x, float y);
  void DrawRect(float x, float y, float w, float h, ::Color color);
  void DrawTextString(const std::string &text, int x, int y, int size,
                      ::Color color);
  void DrawTexturePro(::Texture2D texture, ::Rectangle source, ::Rectangle dest,
                      ::Vector2 origin, float rotation, ::Color tint);
  void DrawTextureRec(::Texture2D texture, ::Rectangle source,
                      ::Vector2 position, ::Color tint);

  bool WindowShouldClose() const;
};

} // namespace criogenio
