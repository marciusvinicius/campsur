#include "application_window.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <cstdio>

namespace criogenio {

struct ApplicationWindow::Impl {
  SDL_Window* window = nullptr;
  bool resized = false;
  int pendingW = 0;
  int pendingH = 0;
};

void ApplicationWindow::PrepareOpenGLAttributes() {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
}

ApplicationWindow::ApplicationWindow(const WindowConfig& config) {
  impl_ = new Impl();

  if (SDL_Init(SDL_INIT_VIDEO) != true) {
    initError_ = SDL_GetError() ? SDL_GetError() : "SDL_Init(SDL_INIT_VIDEO) failed";
    std::fprintf(stderr, "%s\n", initError_.c_str());
    delete impl_;
    impl_ = nullptr;
    return;
  }

  if ((config.features & static_cast<uint32_t>(WindowFeature::OpenGL)) != 0u)
    PrepareOpenGLAttributes();

  SDL_WindowFlags flags = 0;
  if ((config.features & static_cast<uint32_t>(WindowFeature::OpenGL)) != 0u)
    flags |= SDL_WINDOW_OPENGL;
  if ((config.features & static_cast<uint32_t>(WindowFeature::Resizable)) != 0u)
    flags |= SDL_WINDOW_RESIZABLE;
  if ((config.features & static_cast<uint32_t>(WindowFeature::HighPixelDensity)) != 0u)
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

  impl_->window =
      SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
  if (!impl_->window) {
    initError_ = SDL_GetError() ? SDL_GetError() : "SDL_CreateWindow failed";
    std::fprintf(stderr, "%s\n", initError_.c_str());
    SDL_Quit();
    delete impl_;
    impl_ = nullptr;
    return;
  }

  SDL_GetWindowSize(impl_->window, &impl_->pendingW, &impl_->pendingH);
}

ApplicationWindow::~ApplicationWindow() {
  if (impl_) {
    if (impl_->window)
      SDL_DestroyWindow(impl_->window);
    delete impl_;
    impl_ = nullptr;
    SDL_Quit();
  }
}

bool ApplicationWindow::IsValid() const { return impl_ != nullptr && impl_->window != nullptr; }

const char* ApplicationWindow::GetInitError() const {
  return initError_.empty() ? "" : initError_.c_str();
}

void ApplicationWindow::SetRelativeMouseMode(bool enabled) {
  if (impl_ && impl_->window)
    SDL_SetWindowRelativeMouseMode(impl_->window, enabled);
}

void ApplicationWindow::PollEvents() {
  if (!impl_ || !impl_->window)
    return;

  impl_->resized = false;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT)
      quitRequested_ = true;
    if (e.type == SDL_EVENT_WINDOW_RESIZED &&
        e.window.windowID == SDL_GetWindowID(impl_->window)) {
      impl_->resized = true;
      SDL_GetWindowSize(impl_->window, &impl_->pendingW, &impl_->pendingH);
    }
  }
}

bool ApplicationWindow::ConsumeResize(int& outWidth, int& outHeight) {
  if (!impl_ || !impl_->resized)
    return false;
  outWidth = impl_->pendingW;
  outHeight = impl_->pendingH;
  impl_->resized = false;
  return true;
}

void ApplicationWindow::GetSize(int& outWidth, int& outHeight) const {
  if (!impl_ || !impl_->window) {
    outWidth = outHeight = 0;
    return;
  }
  SDL_GetWindowSize(impl_->window, &outWidth, &outHeight);
}

void ApplicationWindow::GetDrawableSize(int& outWidth, int& outHeight) const {
  if (!impl_ || !impl_->window) {
    outWidth = outHeight = 0;
    return;
  }
  if (!SDL_GetWindowSizeInPixels(impl_->window, &outWidth, &outHeight))
    SDL_GetWindowSize(impl_->window, &outWidth, &outHeight);
}

void ApplicationWindow::GetRelativeMouse(float& outDx, float& outDy) const {
  if (!impl_ || !impl_->window) {
    outDx = outDy = 0.f;
    return;
  }
  SDL_GetRelativeMouseState(&outDx, &outDy);
}

void* ApplicationWindow::GetPlatformWindowHandle() const {
  return impl_ ? static_cast<void*>(impl_->window) : nullptr;
}

} // namespace criogenio
