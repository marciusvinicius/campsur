#pragma once

#include "core.h"
#include "event.h"
#include "render.h"
#include "scene.h"

namespace criogenio {

Texture2D CriogenioLoadTexture(const char *file_name);

class Engine {
public:
  Engine(int width, int height, const char *title);
  ~Engine();

  void Run();

  Scene &GetScene();
  EventBus &GetEventBus();
  Renderer &GetRenderer();

protected:
  virtual void OnGUI() {} // Editor overrides this

private:
  // TODO:(maraujo) use smartpointer
  Renderer *renderer;
  Scene *scene;
  EventBus eventBus;
  float previousTime = 0;
};

} // namespace criogenio
