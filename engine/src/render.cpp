#include "renderer.h"

namespace criogenio {

Renderer::Renderer(int width, int height, const std::string &title) {
  InitWindow(width, height, title.c_str());
  SetTargetFPS(60);
}

Renderer::~Renderer() { CloseWindow(); }

void Renderer::BeginFrame() {
  ::BeginDrawing();
  ::ClearBackground(DARKGRAY);
}

void Renderer::EndFrame() { EndDrawing(); }

void Renderer::DrawTexture(Texture2D texture, float x, float y) {
  ::DrawTexture(texture, (int)x, (int)y, WHITE);
}

void Renderer::DrawTextureRec(::Texture2D texture, ::Rectangle source,
                              ::Vector2 position, ::Color tint) {
  ::DrawTextureRec(texture, source, position, tint);
}
void Renderer::DrawTexturePro(::Texture2D texture, ::Rectangle source,
                              ::Rectangle dest, ::Vector2 origin,
                              float rotation, ::Color tint) {
  ::DrawTexturePro(texture, source, dest, origin, rotation, tint);
}

void Renderer::DrawRect(float x, float y, float w, float h, Color color) {
  ::DrawRectangle((int)x, (int)y, (int)w, (int)h, color);
}

void Renderer::DrawTextString(const std::string &text, int x, int y, int size,
                              Color color) {
  ::DrawText(text.c_str(), x, y, size, color);
}

bool Renderer::WindowShouldClose() const { return ::WindowShouldClose(); }

} // namespace criogenio
