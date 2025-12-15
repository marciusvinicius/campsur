#include "snakecontroller.h"
#include "input.h"
#include <cstdlib>

SnakeController::SnakeController(World &world, EventBus &bus)
    : world(world), bus(bus) {
  SpawnSnake();
  SpawnFood();
}

void SnakeController::SpawnSnake() {
  segments.clear();

  for (int i = 0; i < 3; i++) {
    int entityId = world.CreateEntity("SnakePart");
    criogenio::Transform &transform =
        world.GetComponent<criogenio::Transform>(entityId);
    transform.x = 200 - i * 20;
    transform.y = 200;
    segments.push_back(entityId);
  }
}

void SnakeController::SpawnFood() {
  int entityID = world.CreateEntity("Food");
  criogenio::Transform &transform =
      world.GetComponent<criogenio::Transform>(entityID);
  transform.x = (rand() % 30) * 20;
  transform.y = (rand() % 20) * 20;
  foodId = entityID;
}

void SnakeController::HandleInput() {
  if (Input::IsKeyPressed(KEY_UP) && direction != DOWN)
    direction = UP;
  else if (Input::IsKeyPressed(KEY_DOWN) && direction != UP)
    direction = DOWN;
  else if (Input::IsKeyPressed(KEY_LEFT) && direction != RIGHT)
    direction = LEFT;
  else if (Input::IsKeyPressed(KEY_RIGHT) && direction != LEFT)
    direction = RIGHT;
}

void SnakeController::Update(float dt) {
  HandleInput();
  timer += dt;

  if (timer >= moveInterval) {
    Move();
    timer = 0;
  }
}

bool SnakeController::CheckSelfCollision() {
  auto &head = world.GetComponent<criogenio::Transform>(segments[0]);

  criogenio::Transform &headTransform =
      world.GetComponent<criogenio::Transform>(segments[0]);

  for (int i = 1; i < segments.size(); i++) {
    criogenio::Transform &part =
        world.GetComponent<criogenio::Transform>(segments[i]);
    if ((int)head.x == (int)part.x && (int)head.y == (int)part.y) {
      return true;
    }
  }
  return false;
}

void SnakeController::Move() {
  // Move body segments
  for (int i = segments.size() - 1; i > 0; i--) {
    criogenio::Transform &curr =
        world.GetComponent<criogenio::Transform>(segments[i]);
    criogenio::Transform &prev =
        world.GetComponent<criogenio::Transform>(segments[i - 1]);

    curr.x = prev.x;
    curr.y = prev.y;
  }
  // Move head
  auto &head = world.GetComponent<criogenio::Transform>(segments[0]);

  switch (direction) {
  case UP:
    head.y -= 20;
    break;
  case DOWN:
    head.y += 20;
    break;
  case LEFT:
    head.x -= 20;
    break;
  case RIGHT:
    head.x += 20;
    break;
  }

  // Check self collision
  if (CheckSelfCollision()) {
    bus.Emit(Event(EventType::Custom)); // Game over
    return;
  }

  // Check food
  auto &food = world.GetComponent<criogenio::Transform>(foodId);

  if ((int)head.x == (int)food.x && (int)head.y == (int)food.y) {
    // Grow snake
    auto &tail = world.GetComponent<criogenio::Transform>(segments.back());
    int newSeg = world.CreateEntity("SnakePart");
    world.AddComponent<criogenio::Transform>(newSeg, tail.x, tail.y);

    // Respawn food
    world.DeleteEntity(foodId);
    SpawnFood();
  }
}
