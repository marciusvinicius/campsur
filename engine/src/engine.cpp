#include "engine.h"
#include "asset_manager.h"
#include "component_factory.h"
#include "raylib.h"
#include "raymath.h"
#include "resources.h"

namespace criogenio {

Texture2D CriogenioLoadTexture(const char *file_name) {
  return LoadTexture(file_name);
}

Engine::Engine(int width, int height, const char *title) : width(width) {

  RegisterCoreComponents();
  renderer = new Renderer(width, height, title);
  world = new World2();

  // Register a simple texture loader that wraps raylib's LoadTexture
  AssetManager::instance().registerLoader<TextureResource>(
      [](const std::string &p) -> std::shared_ptr<TextureResource> {
        Texture2D t = CriogenioLoadTexture(p.c_str());
        if (t.id == 0) {
          // Failed to load texture, log error
          TraceLog(LOG_WARNING, "AssetManager: Failed to load texture: %s",
                   p.c_str());
          return nullptr;
        }
        return std::make_shared<TextureResource>(p, t);
      });
}

Engine::~Engine() {
  delete world;
  delete renderer;
}

World2 &Engine::GetWorld() { return *world; }
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

  ComponentFactory::Register("Transform", [](World2 &w, int e) {
    return &w.AddComponent<Transform>(e);
  });

  ComponentFactory::Register("AnimatedSprite", [](World2 &w, int e) {
    return &w.AddComponent<AnimatedSprite>(e);
  });

  ComponentFactory::Register("Controller", [](World2 &w, int e) {
    return &w.AddComponent<Controller>(e);
  });

  ComponentFactory::Register("AnimationState", [](World2 &w, int e) {
    return &w.AddComponent<AnimationState>(e);
  });

  ComponentFactory::Register("AIController", [](World2 &w, int e) {
    return &w.AddComponent<AIController>(e);
  });

  ComponentFactory::Register(
      "Name", [](World2 &w, int e) { return &w.AddComponent<Name>(e, ""); });
}
} // namespace criogenio
