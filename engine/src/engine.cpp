#include "engine.h"
#include "raylib.h"
#include "raymath.h"

namespace criogenio {

Texture2D CriogenioLoadTexture(const char *file_name) {
  return LoadTexture(file_name);
}

std::unordered_map<std::string, ComponentFactoryFn> &
ComponentFactory::Registry() {
  static std::unordered_map<std::string, ComponentFactoryFn> registry;
  return registry;
}

void ComponentFactory::Register(const std::string &type,
                                ComponentFactoryFn fn) {
  Registry()[type] = std::move(fn);
}

Component *ComponentFactory::Create(const std::string &type, World &world,
                                    int entity) {
  auto it = Registry().find(type);
  if (it == Registry().end())
    return nullptr;

  return it->second(world, entity);
}

Engine::Engine(int width, int height, const char *title) : width(width) {
  renderer = new Renderer(width, height, title);
  world = new World();
  RegisterCoreComponents();
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

// #TODO:(maraujo) Move this to Engine
Vector2 Engine::GetMouseWorld() {
  return GetScreenToWorld2D(GetMousePosition(), GetWorld().maincamera);
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
}
} // namespace criogenio
