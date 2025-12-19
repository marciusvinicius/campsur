#include "engine.h"
#include "raylib.h"
#include "raymath.h"

namespace criogenio {
Texture2D CriogenioLoadTexture(const char *file_name) {
  return LoadTexture(file_name);
}

Engine::Engine(int width, int height, const char *title) : width(width) {
  renderer = new Renderer(width, height, title);
  world = new World();
}

Engine::~Engine() {
  delete world;
  delete renderer;
}

World &Engine::GetWorld() { return *world; }
EventBus &Engine::GetEventBus() { return eventBus; }
Renderer &Engine::GetRenderer() { return *renderer; }

void Engine::Run() {
  previousTime = GetTime();
  while (!renderer->WindowShouldClose()) {
    float now = GetTime();
    float dt = now - previousTime;
    previousTime = now;
    world->Update(dt);
    renderer->BeginFrame();
    world->Render(*renderer);
    // Just call GUI hook — unused in game runtime
    OnGUI();
    renderer->EndFrame();
  }
}
} // namespace criogenio
