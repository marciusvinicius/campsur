#include "engine.h"
#include "snake_renderer.h"
#include "snakecontroller.h"
#include <iostream>

using namespace criogenio;

int main() {
  Engine engine(640, 480, "Snake Using Custom Engine");

  World &World = engine.GetWorld();
  EventBus &bus = engine.GetEventBus();

  SnakeController snake(World, bus);

  // Ensure the world has a valid camera for 2D rendering
  Camera2D cam = {0};
  cam.target = {200, 200}; // Center camera on snake spawn area
  cam.offset = {(float)engine.width / 2.0f, (float)engine.height / 2.0f};
  cam.rotation = 0.0f;
  cam.zoom = 2.0f; // Zoom in to see 20x20 blocks clearly
  World.AttachCamera2D(cam);

  // Add simple renderer for snake and food
  World.AddSystem<SnakeRenderSystem>(World);

  // Listen for game over
  bus.Subscribe(EventType::Custom, [&](const Event &e) {
    std::cout << "Game Over!" << std::endl;
  });

  // Inject update function into World
  World.OnUpdate([&](float dt) { snake.Update(dt); });

  engine.Run();
  return 0;
}
