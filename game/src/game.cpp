#include "game.h"
#include "input.h"
#include "keys.h"

void MySystem::Update(float dt) { (void)dt; }

void MySystem::Render(criogenio::Renderer &renderer) { (void)renderer; }

void GameEngine::OnFrame(float dt) {
  (void)dt;
  if (GetNetworkMode() != criogenio::NetworkMode::Client)
    return;
  criogenio::PlayerInput input = {};
  using namespace criogenio;
  if (Input::IsKeyDown(static_cast<int>(Key::Right)) || Input::IsKeyDown(static_cast<int>(Key::D)))
    input.move_x += 1.f;
  if (Input::IsKeyDown(static_cast<int>(Key::Left)) || Input::IsKeyDown(static_cast<int>(Key::A)))
    input.move_x -= 1.f;
  if (Input::IsKeyDown(static_cast<int>(Key::Up)) || Input::IsKeyDown(static_cast<int>(Key::W)))
    input.move_y -= 1.f;
  if (Input::IsKeyDown(static_cast<int>(Key::Down)) || Input::IsKeyDown(static_cast<int>(Key::S)))
    input.move_y += 1.f;
  SendInputAsClient(input);
}
