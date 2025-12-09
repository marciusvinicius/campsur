#include "engine.h"

using namespace criogenio;
int main_game() {
  Engine engine(800, 600, "My Game Engine");

  auto &scene = engine.GetScene();
  Entity &player = scene.CreateEntity("Player");
  player.transform.x = 200;
  player.transform.y = 100;

  // Load sprite (optional)
  player.sprite.texture = CriogenioLoadTexture("player.png");
  player.sprite.loaded = true;

  engine.Run();
  return 0;
}
