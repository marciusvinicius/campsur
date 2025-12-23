#include "engine.h"
#include "component_factory.h"
#include "raylib.h"
#include "raymath.h"

namespace criogenio {

Texture2D CriogenioLoadTexture(const char *file_name) {
  return LoadTexture(file_name);
}

Engine::Engine(int width, int height, const char *title) : width(width) {

  RegisterCoreComponents();
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

void Engine::RegisterCoreComponents() {

  ComponentFactory::Register("Transform", [](World &w, int e) {
    return w.AddComponent<Transform>(e);
  });

  ComponentFactory::Register("AnimatedSprite", [](World &w, int e) {
    return w.AddComponent<AnimatedSprite>(e);
  });

  ComponentFactory::Register("Controller", [](World &w, int e) {
    return w.AddComponent<Controller>(e);
  });

  ComponentFactory::Register("AnimationState", [](World &w, int e) {
    return w.AddComponent<AnimationState>(e);
  });

  ComponentFactory::Register("AIController", [](World &w, int e) {
    return w.AddComponent<AIController>(e);
  });

  ComponentFactory::Register(
      "Name", [](World &w, int e) { return w.AddComponent<Name>(e, ""); });
}
} // namespace criogenio
