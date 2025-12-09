#include "engine.h"
#include "snakecontroller.h"
#include <iostream>

using namespace criogenio;

int main_snake() {
  Engine engine(640, 480, "Snake Using Custom Engine");

  Scene &scene = engine.GetScene();
  EventBus &bus = engine.GetEventBus();

  SnakeController snake(scene, bus);

  // Listen for game over
  bus.Subscribe(EventType::Custom, [&](const Event &e) {
    std::cout << "Game Over!" << std::endl;
  });

  // Inject update function into scene
  scene.OnUpdate([&](float dt) { snake.Update(dt); });

  engine.Run();
  return 0;
}
