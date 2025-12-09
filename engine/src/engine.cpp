#include "engine.h"
#include "raylib.h"
#include "raymath.h"
#include "system.h"

namespace criogenio {

Texture2D CriogenioLoadTexture(const char *file_name) {
  return LoadTexture(file_name);
}

Engine::Engine(int width, int height, const char *title) {
  renderer = new Renderer(width, height, title);
  scene = new Scene();
}

Engine::~Engine() {
  delete scene;
  delete renderer;
}

Scene &Engine::GetScene() { return *scene; }
EventBus &Engine::GetEventBus() { return eventBus; }
Renderer &Engine::GetRenderer() { return *renderer; }

void Engine::Run() {
  previousTime = GetTime();

  while (!renderer->WindowShouldClose()) {
    float now = GetTime();
    float dt = now - previousTime;
    previousTime = now;

    scene->Update(dt);
    renderer->BeginFrame();
    // TODO:(maraujo) think more abount this pointers
    scene->Render(*renderer);
    // Just call GUI hook — unused in game runtime
    OnGUI();
    renderer->EndFrame();
  }
}
} // namespace criogenio
