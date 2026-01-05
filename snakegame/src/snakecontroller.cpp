#include "snakecontroller.h"
#include "input.h"
#include <cstdlib>

using namespace criogenio;

SnakeController::SnakeController(World &world, EventBus &bus)
    : world(world), bus(bus) {
  SpawnSnake();
  SpawnFood();
}

void SnakeController::SpawnSnake() {
  segments.clear();

  for (int i = 0; i < 3; i++) {
    int entityId = world.CreateEntity("SnakePart");
    world.AddComponent<criogenio::Transform>(entityId, 200 - i * 20, 200);
    world.AddComponent<criogenio::Name>(entityId, "SnakePart");
    segments.push_back(entityId);
    std::cout << "Spawned snake part " << i << " at entity " << entityId
              << " pos (" << (200 - i * 20) << ", 200)" << std::endl;
  }
}

void SnakeController::SpawnFood() {
  int entityID = world.CreateEntity("Food");
  float fx = (rand() % 30) * 20;
  float fy = (rand() % 20) * 20;
  world.AddComponent<criogenio::Transform>(entityID, fx, fy);
  world.AddComponent<criogenio::Name>(entityID, "Food");
  foodId = entityID;
  std::cout << "Spawned food at entity " << entityID << " pos (" << fx << ", "
            << fy << ")" << std::endl;
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
  auto *head = world.GetComponent<criogenio::Transform>(segments[0]);
  if (!head)
    return false;

  for (int i = 1; i < segments.size(); i++) {
    auto *part = world.GetComponent<criogenio::Transform>(segments[i]);
    if (!part)
      continue;
    if ((int)head->x == (int)part->x && (int)head->y == (int)part->y) {
      return true;
    }
  }
  return false;
}

void SnakeController::Move() {
  // Move body segments
  for (int i = segments.size() - 1; i > 0; i--) {
    auto *curr = world.GetComponent<criogenio::Transform>(segments[i]);
    auto *prev = world.GetComponent<criogenio::Transform>(segments[i - 1]);
    if (!curr || !prev)
      continue;

    curr->x = prev->x;
    curr->y = prev->y;
  }
  // Move head
  auto *head = world.GetComponent<criogenio::Transform>(segments[0]);
  if (!head)
    return;

  switch (direction) {
  case UP:
    head->y -= 20;
    break;
  case DOWN:
    head->y += 20;
    break;
  case LEFT:
    head->x -= 20;
    break;
  case RIGHT:
    head->x += 20;
    break;
  }

  // Check self collision
  if (CheckSelfCollision()) {
    bus.Emit(Event(EventType::Custom)); // Game over
    return;
  }

  // Check food
  auto *food = world.GetComponent<criogenio::Transform>(foodId);
  if (food && (int)head->x == (int)food->x && (int)head->y == (int)food->y) {
    // Grow snake
    auto *tail = world.GetComponent<criogenio::Transform>(segments.back());
    int newSeg = world.CreateEntity("SnakePart");
    if (tail)
      world.AddComponent<criogenio::Transform>(newSeg, tail->x, tail->y);
    else
      world.AddComponent<criogenio::Transform>(newSeg, 0, 0);

    // Respawn food
    world.DeleteEntity(foodId);
    SpawnFood();
  }
}
