#pragma once
#include "event.h"
#include "world2.h"
#include <deque>

class SnakeController {
public:
  SnakeController(criogenio::World2 &world, criogenio::EventBus &bus);

  void Update(float dt);

private:
  criogenio::World2 &world;
  criogenio::EventBus &bus;

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
