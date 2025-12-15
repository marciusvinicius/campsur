#pragma once
#include "event.h"
#include "world.h"
#include <deque>

using namespace criogenio;

class SnakeController {
public:
  SnakeController(World &world, EventBus &bus);

  void Update(float dt);

private:
  World &world;
  EventBus &bus;

  float timer = 0.0f;
  float moveInterval = 0.15f;

  enum Dir { UP, DOWN, LEFT, RIGHT };
  Dir direction = RIGHT;

  std::deque<int> segments; // list of entity IDs of the snake body

  int foodId = -1;

  void SpawnSnake();
  void SpawnFood();

  void HandleInput();
  void Move();
  bool CheckSelfCollision();
};
