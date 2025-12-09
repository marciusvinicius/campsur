#include "snakecontroller.h"
#include "input.h"
#include <cstdlib>

SnakeController::SnakeController(Scene &scene, EventBus &bus)
    : scene(scene), bus(bus) {
  SpawnSnake();
  SpawnFood();
}

void SnakeController::SpawnSnake() {
  segments.clear();

  for (int i = 0; i < 3; i++) {
    auto &e = scene.CreateEntity("SnakePart");
    e.transform.x = 100 - (i * 20);
    e.transform.y = 100;
    segments.push_back(e.id);
  }
}

void SnakeController::SpawnFood() {
  auto &f = scene.CreateEntity("Food");
  f.transform.x = (rand() % 20) * 20;
  f.transform.y = (rand() % 20) * 20;
  foodId = f.id;
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
  auto &head = scene.GetEntityById(segments[0]).transform;

  for (int i = 1; i < segments.size(); i++) {
    auto &part = scene.GetEntityById(segments[i]).transform;
    if ((int)head.x == (int)part.x && (int)head.y == (int)part.y) {
      return true;
    }
  }
  return false;
}

void SnakeController::Move() {
  // Move body segments
  for (int i = segments.size() - 1; i > 0; i--) {
    auto &curr = scene.GetEntityById(segments[i]).transform;
    auto &prev = scene.GetEntityById(segments[i - 1]).transform;

    curr.x = prev.x;
    curr.y = prev.y;
  }

  // Move head
  auto &head = scene.GetEntityById(segments[0]).transform;

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
  auto &food = scene.GetEntityById(foodId).transform;
  if ((int)head.x == (int)food.x && (int)head.y == (int)food.y) {
    // Grow snake
    auto &tail = scene.GetEntityById(segments.back()).transform;
    auto &newSeg = scene.CreateEntity("SnakePart");
    newSeg.transform.x = tail.x;
    newSeg.transform.y = tail.y;
    segments.push_back(newSeg.id);

    // Respawn food
    scene.DeleteEntity(foodId);
    SpawnFood();
  }
}
