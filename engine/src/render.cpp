#include "render.h"
#include "graphics_types.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <cmath>
#include <cstring>

namespace criogenio {

namespace {

struct RendererImpl {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  int viewportW = 0;
  int viewportH = 0;
  bool cameraActive = false;
  Camera2D camera;
  bool quitRequested = false;
  SDL_Texture* currentRenderTarget = nullptr;  // when set, viewport follows target size
};

static RendererImpl* s_impl = nullptr;

// World to screen when camera is active
void WorldToScreen(float worldX, float worldY, const Camera2D& cam, int vpW,
                   int vpH, float& outX, float& outY) {
  float halfW = vpW * 0.5f;
  float halfH = vpH * 0.5f;
  outX = (worldX - cam.target.x) * cam.zoom + cam.offset.x + halfW;
  outY = (worldY - cam.target.y) * cam.zoom + cam.offset.y + halfH;
}

void SDL_SetRenderDrawColorFromColor(SDL_Renderer* r, Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

} // namespace

Renderer::Renderer(int width, int height, const std::string& title) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return;
  }
  s_impl = new RendererImpl();
  s_impl->viewportW = width;
  s_impl->viewportH = height;
  s_impl->window =
      SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_RESIZABLE);
  if (!s_impl->window) {
    delete s_impl;
    s_impl = nullptr;
    return;
  }
  s_impl->renderer = SDL_CreateRenderer(s_impl->window, nullptr);
  if (!s_impl->renderer) {
    SDL_DestroyWindow(s_impl->window);
    delete s_impl;
    s_impl = nullptr;
  }
}

Renderer::~Renderer() {
  if (s_impl) {
    if (s_impl->renderer)
      SDL_DestroyRenderer(s_impl->renderer);
    if (s_impl->window)
      SDL_DestroyWindow(s_impl->window);
    delete s_impl;
    s_impl = nullptr;
  }
  SDL_Quit();
}

void Renderer::SetViewport(int width, int height) {
  if (s_impl) {
    s_impl->viewportW = width;
    s_impl->viewportH = height;
  }
}

void Renderer::BeginCamera2D(const Camera2D& camera) {
  if (s_impl) {
    s_impl->cameraActive = true;
    s_impl->camera = camera;
  }
}

void Renderer::EndCamera2D() {
  if (s_impl)
    s_impl->cameraActive = false;
}

void Renderer::BeginFrame() {
  if (!s_impl || !s_impl->renderer)
    return;
  SDL_SetRenderDrawColor(s_impl->renderer, 64, 64, 64, 255);
  SDL_RenderClear(s_impl->renderer);
}

void Renderer::EndFrame() {
  if (s_impl && s_impl->renderer)
    SDL_RenderPresent(s_impl->renderer);
}

static void drawRectImpl(SDL_Renderer* r, float x, float y, float w, float h,
                         Color color, bool fill, int vpW, int vpH,
                         bool cameraActive, const Camera2D* cam) {
  float x1 = x, y1 = y, x2 = x + w, y2 = y + h;
  if (cameraActive && cam) {
    float sx1, sy1, sx2, sy2;
    WorldToScreen(x, y, *cam, vpW, vpH, sx1, sy1);
    WorldToScreen(x + w, y + h, *cam, vpW, vpH, sx2, sy2);
    x1 = sx1;
    y1 = sy1;
    x2 = sx2;
    y2 = sy2;
  }
  SDL_FRect fr = {x1, y1, x2 - x1, y2 - y1};
  SDL_SetRenderDrawColorFromColor(r, color);
  if (fill)
    SDL_RenderFillRect(r, &fr);
  else
    SDL_RenderRect(r, &fr);
}

void Renderer::DrawRect(float x, float y, float w, float h, Color color) {
  if (!s_impl || !s_impl->renderer)
    return;
  drawRectImpl(s_impl->renderer, x, y, w, h, color, true, s_impl->viewportW,
               s_impl->viewportH, s_impl->cameraActive,
               s_impl->cameraActive ? &s_impl->camera : nullptr);
}

void Renderer::DrawRectOutline(float x, float y, float w, float h, Color color) {
  if (!s_impl || !s_impl->renderer)
    return;
  drawRectImpl(s_impl->renderer, x, y, w, h, color, false, s_impl->viewportW,
               s_impl->viewportH, s_impl->cameraActive,
               s_impl->cameraActive ? &s_impl->camera : nullptr);
}

void Renderer::DrawCircle(float x, float y, float r, Color color) {
  if (!s_impl || !s_impl->renderer)
    return;
  float cx = x, cy = y;
  if (s_impl->cameraActive) {
    float sx, sy;
    WorldToScreen(x, y, s_impl->camera, s_impl->viewportW, s_impl->viewportH, sx,
                  sy);
    cx = sx;
    cy = sy;
    r *= s_impl->camera.zoom;
  }
  SDL_SetRenderDrawColorFromColor(s_impl->renderer, color);
  // SDL3: approximate circle with filled rects or use a helper
  const int segments = 32;
  for (int i = 0; i < segments; i++) {
    float a1 = (float)i * (2.0f * 3.14159265f / segments);
    float a2 = (float)(i + 1) * (2.0f * 3.14159265f / segments);
    float x1 = cx + r * cosf(a1);
    float y1 = cy + r * sinf(a1);
    float x2 = cx + r * cosf(a2);
    float y2 = cy + r * sinf(a2);
    SDL_RenderLine(s_impl->renderer, x1, y1, x2, y2);
  }
}

void Renderer::DrawLine(float x1, float y1, float x2, float y2, Color color) {
  if (!s_impl || !s_impl->renderer)
    return;
  if (s_impl->cameraActive) {
    float sx1, sy1, sx2, sy2;
    WorldToScreen(x1, y1, s_impl->camera, s_impl->viewportW, s_impl->viewportH,
                  sx1, sy1);
    WorldToScreen(x2, y2, s_impl->camera, s_impl->viewportW, s_impl->viewportH,
                  sx2, sy2);
    x1 = sx1;
    y1 = sy1;
    x2 = sx2;
    y2 = sy2;
  }
  SDL_SetRenderDrawColorFromColor(s_impl->renderer, color);
  SDL_RenderLine(s_impl->renderer, x1, y1, x2, y2);
}

void Renderer::DrawGrid(int slices, float spacing) {
  if (!s_impl || !s_impl->renderer)
    return;
  Color gridColor{80, 80, 80, 255};
  float extent = (float)slices * spacing * 0.5f;
  for (int i = -slices / 2; i <= slices / 2; i++) {
    float p = (float)i * spacing;
    DrawLine(-extent, p, extent, p, gridColor);
    DrawLine(p, -extent, p, extent, gridColor);
  }
}

void Renderer::DrawTextString(const std::string& text, int x, int y, int size,
                              Color color) {
  (void)text;
  (void)x;
  (void)y;
  (void)size;
  (void)color;
  // SDL3 has no built-in text; would need SDL_ttf. Stub for now.
}

void Renderer::DrawTexture(TextureHandle texture, float x, float y) {
  if (!s_impl || !s_impl->renderer || !texture.valid())
    return;
  SDL_Texture* tex = (SDL_Texture*)texture.opaque;
  int w = 0, h = 0;
  SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
  float dx = x, dy = y;
  if (s_impl->cameraActive) {
    WorldToScreen(x, y, s_impl->camera, s_impl->viewportW, s_impl->viewportH, dx,
                  dy);
  }
  SDL_FRect dst = {dx, dy, (float)w, (float)h};
  SDL_RenderTexture(s_impl->renderer, tex, nullptr, &dst);
}

void Renderer::DrawTextureRec(TextureHandle texture, Rect source, Vec2 position,
                              Color tint) {
  if (!s_impl || !s_impl->renderer || !texture.valid())
    return;
  SDL_Texture* tex = (SDL_Texture*)texture.opaque;
  SDL_FRect src = {source.x, source.y, source.width, source.height};
  float dx = position.x, dy = position.y;
  if (s_impl->cameraActive) {
    WorldToScreen(position.x, position.y, s_impl->camera, s_impl->viewportW,
                  s_impl->viewportH, dx, dy);
  }
  SDL_FRect dst = {dx, dy, source.width, source.height};
  SDL_SetRenderTextureColorMod(tex, tint.r, tint.g, tint.b);
  SDL_SetRenderTextureAlphaMod(tex, tint.a);
  SDL_RenderTexture(s_impl->renderer, tex, &src, &dst);
  SDL_SetRenderTextureColorMod(tex, 255, 255, 255);
  SDL_SetRenderTextureAlphaMod(tex, 255);
}

void Renderer::DrawTexturePro(TextureHandle texture, Rect source, Rect dest,
                              Vec2 origin, float rotation, Color tint) {
  (void)origin;
  (void)rotation;
  // Simplified: ignore rotation and origin, draw scaled
  if (!s_impl || !s_impl->renderer || !texture.valid())
    return;
  SDL_Texture* tex = (SDL_Texture*)texture.opaque;
  SDL_FRect src = {source.x, source.y, source.width, source.height};
  float dx = dest.x, dy = dest.y;
  if (s_impl->cameraActive) {
    WorldToScreen(dest.x, dest.y, s_impl->camera, s_impl->viewportW,
                  s_impl->viewportH, dx, dy);
  }
  SDL_FRect dst = {dx, dy, dest.width, dest.height};
  SDL_SetRenderTextureColorMod(tex, tint.r, tint.g, tint.b);
  SDL_SetRenderTextureAlphaMod(tex, tint.a);
  SDL_RenderTexture(s_impl->renderer, tex, &src, &dst);
  SDL_SetRenderTextureColorMod(tex, 255, 255, 255);
  SDL_SetRenderTextureAlphaMod(tex, 255);
}

void Renderer::ProcessEvents(std::function<void(const void*)>* onEvent) {
  if (!s_impl || !s_impl->window)
    return;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (onEvent)
      (*onEvent)(&e);
    if (e.type == SDL_EVENT_QUIT)
      s_impl->quitRequested = true;
    if (e.type == SDL_EVENT_WINDOW_RESIZED && e.window.windowID == SDL_GetWindowID(s_impl->window)) {
      int w = 0, h = 0;
      SDL_GetWindowSize(s_impl->window, &w, &h);
      s_impl->viewportW = w;
      s_impl->viewportH = h;
    }
  }
}

bool Renderer::WindowShouldClose() const {
  return s_impl ? s_impl->quitRequested : true;
}

int Renderer::GetViewportWidth() const {
  return s_impl ? s_impl->viewportW : 0;
}
int Renderer::GetViewportHeight() const {
  return s_impl ? s_impl->viewportH : 0;
}

Vec2 Renderer::GetMousePosition() const {
  Vec2 out{0, 0};
  if (!s_impl || !s_impl->window)
    return out;
  float x = 0, y = 0;
  SDL_GetMouseState(&x, &y);
  out.x = x;
  out.y = y;
  return out;
}

void* Renderer::GetWindowHandle() const {
  return s_impl ? s_impl->window : nullptr;
}

void* Renderer::GetRendererHandle() const {
  return s_impl ? s_impl->renderer : nullptr;
}

TextureHandle Renderer::CreateRenderTarget(int width, int height) {
  TextureHandle out{};
  if (!s_impl || !s_impl->renderer || width <= 0 || height <= 0)
    return out;
  SDL_Texture* tex = SDL_CreateTexture(s_impl->renderer, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET, width, height);
  if (!tex)
    return out;
  out.opaque = tex;
  out.width = width;
  out.height = height;
  return out;
}

void Renderer::SetRenderTarget(TextureHandle target) {
  if (!s_impl || !s_impl->renderer)
    return;
  SDL_Texture* tex = target.valid() ? (SDL_Texture*)target.opaque : nullptr;
  if (SDL_SetRenderTarget(s_impl->renderer, tex) == 0) {
    s_impl->currentRenderTarget = tex;
    if (tex) {
      s_impl->viewportW = target.width;
      s_impl->viewportH = target.height;
    } else {
      int w = 0, h = 0;
      SDL_GetWindowSize(s_impl->window, &w, &h);
      s_impl->viewportW = w;
      s_impl->viewportH = h;
    }
  }
}

void Renderer::UnsetRenderTarget() {
  if (!s_impl || !s_impl->renderer)
    return;
  SDL_SetRenderTarget(s_impl->renderer, nullptr);
  s_impl->currentRenderTarget = nullptr;
  int w = 0, h = 0;
  SDL_GetWindowSize(s_impl->window, &w, &h);
  s_impl->viewportW = w;
  s_impl->viewportH = h;
}

void Renderer::DestroyRenderTarget(TextureHandle* target) {
  if (!target || !target->opaque)
    return;
  if (s_impl && s_impl->currentRenderTarget == target->opaque)
    UnsetRenderTarget();
  SDL_DestroyTexture((SDL_Texture*)target->opaque);
  target->opaque = nullptr;
  target->width = 0;
  target->height = 0;
}

TextureHandle Renderer::LoadTexture(const std::string& path) {
  TextureHandle out{};
  if (!s_impl || !s_impl->renderer || path.empty())
    return out;
  SDL_Surface* surf = SDL_LoadPNG(path.c_str());
  if (!surf)
    return out;
  SDL_Texture* tex = SDL_CreateTextureFromSurface(s_impl->renderer, surf);
  SDL_DestroySurface(surf);
  if (!tex)
    return out;
  int w = 0, h = 0;
  SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
  out.opaque = tex;
  out.width = w;
  out.height = h;
  return out;
}

void Renderer::UnloadTexture(TextureHandle* handle) {
  if (!handle || !handle->opaque)
    return;
  SDL_DestroyTexture((SDL_Texture*)handle->opaque);
  handle->opaque = nullptr;
  handle->width = 0;
  handle->height = 0;
}

} // namespace criogenio
