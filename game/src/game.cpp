#include "game.h"
#include "raylib.h"

void MySystem::Update(float dt) { (void)dt; }

void MySystem::Render(criogenio::Renderer &renderer) { (void)renderer; }

void GameEngine::OnFrame(float dt) {
  (void)dt;
  if (GetNetworkMode() != criogenio::NetworkMode::Client)
    return;
  criogenio::PlayerInput input = {};
  if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
    input.move_x += 1.f;
  if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
    input.move_x -= 1.f;
  if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
    input.move_y -= 1.f;
  if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
    input.move_y += 1.f;
  SendInputAsClient(input);
}
