#pragma once

#include "core.h"
#include "event.h"
#include "raylib.h"
#include "render.h"
#include "world2.h"

namespace criogenio {

Texture2D CriogenioLoadTexture(const char *file_name);

class Engine {
public:
  int width;
  int height;
  Engine(int width, int height, const char *title);
  ~Engine();

  void Run();

  World2 &GetWorld();
  EventBus &GetEventBus();
  Renderer &GetRenderer();
  Vector2 GetMouseWorld();

  void RegisterCoreComponents();

protected:
  virtual void OnGUI() {} // Editor overrides this

private:
  // TODO:(maraujo) use smartpointer
  Renderer *renderer;
  World2 *world;
  EventBus eventBus;
  float previousTime = 0;
};

} // namespace criogenio
