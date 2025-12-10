#include "engine.h"
#include "game.h"
// TODO:(maraujo) remove this and create this on engine
#include "raylib.h"

using namespace criogenio;

int main() {
  Engine engine(InitialWidth, InitialHeight, "Ways of the Truth");

  auto &scene = engine.GetScene();
  scene.CreateTerrain2D("MainTerrain", "game/assets/terrain.jpg");

  auto maincamera = Camera2D{};
  maincamera.target = {0.0f, 0.0f};
  maincamera.offset = {InitialWidth / 2.0f, InitialHeight / 2.0f}; // IMPORTANT
  maincamera.zoom = 1.0f;

  scene.AttachCamera2D(maincamera);

  Entity &player = scene.CreateEntity("Player");

  player.transform.x = 200;
  player.transform.y = 100;

  AnimatedSprite animatedSprite;
  animatedSprite.path = "game/assets/body_man_";
  // Load frames for animated sprite
  for (int i = 1; i <= 4; ++i) {
    std::string framePath = animatedSprite.path + std::to_string(i) + ".png";
    Texture2D frameTexture = CriogenioLoadTexture(framePath);
    animatedSprite.frames.push_back(frameTexture);
  }
  animatedSprite.loaded = true;

  scene.AttachCompoentntToEntity(player.id, animatedSprite);

  engine.Run();
  return 0;
}
