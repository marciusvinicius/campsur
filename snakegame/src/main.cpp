#include "engine.h"
#include "snakecontroller.h"
#include <iostream>

using namespace criogenio;

int main() {
  Engine engine(640, 480, "Snake Using Custom Engine");

  World &World = engine.GetWorld();
  EventBus &bus = engine.GetEventBus();

  SnakeController snake(World, bus);

  // Listen for game over
  bus.Subscribe(EventType::Custom, [&](const Event &e) {
    std::cout << "Game Over!" << std::endl;
  });

  // Inject update function into World
  World.OnUpdate([&](float dt) { snake.Update(dt); });

  engine.Run();
  return 0;
}
