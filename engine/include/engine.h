#pragma once

#include "core.h"
#include "event.h"
#include "render.h"
#include "world.h"

namespace criogenio {

Texture2D CriogenioLoadTexture(const char *file_name);

/*

        Start to think about how to serialize the World, for now we dont have a
   simple way to build the Worlds and then load it from a file that should
   include entities and components

*/

class Engine {
public:
  int width;
  int height;
  Engine(int width, int height, const char *title);
  ~Engine();

  void Run();

  World &GetWorld();
  EventBus &GetEventBus();
  Renderer &GetRenderer();

protected:
  virtual void OnGUI() {} // Editor overrides this

private:
  // TODO:(maraujo) use smartpointer
  Renderer *renderer;
  World *world;
  EventBus eventBus;
  float previousTime = 0;
};

} // namespace criogenio
