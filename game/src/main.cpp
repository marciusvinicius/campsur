#include "components.h"
#include "engine.h"
#include "game.h"
#include "world.h"

// TODO:(maraujo) remove this and create this on engine
#include "raylib.h"

using namespace criogenio;

int main() {
  Engine engine(InitialWidth, InitialHeight, "Ways of the Truth");

  auto &World = engine.GetWorld();
  World.CreateTerrain2D("MainTerrain", "game/assets/terrain.jpg");

  World.AddSystem<criogenio::MovementSystem>(World);
  World.AddSystem<criogenio::AnimationSystem>(World);
  World.AddSystem<criogenio::RenderSystem>(World);
  World.AddSystem<criogenio::AIMovementSystem>(World);

  auto maincamera = Camera2D{};
  maincamera.target = {0.0f, 0.0f};
  maincamera.offset = {InitialWidth / 2.0f, InitialHeight / 2.0f}; // IMPORTANT
  maincamera.zoom = 1.0f;

  World.AttachCamera2D(maincamera);

  int entityId = World.CreateEntity("Player");
  World.AddComponent<criogenio::Transform>(entityId);
  World.AddComponent<criogenio::Transform>(entityId, 0.0f, 0.0f);
  auto path = "editor/assets/Woman/woman.png";
  auto texture = LoadTexture(path);

  /// Make this data-driven later
  std::vector<Rectangle> idleDown = {
      {0, 0, 64, 128},   {64, 0, 64, 128},  {128, 0, 64, 128},
      {192, 0, 64, 128}, {256, 0, 64, 128}, {320, 0, 64, 128},
      {384, 0, 64, 128}, {448, 0, 64, 128}, {512, 0, 64, 128},
  };

  std::vector<Rectangle> idleLeft = {
      {0, 128, 64, 128},   {64, 128, 64, 128},  {128, 128, 64, 128},
      {192, 128, 64, 128}, {256, 128, 64, 128}, {320, 128, 64, 128},
      {384, 128, 64, 128}, {448, 128, 64, 128}, {512, 128, 64, 128},
  };

  std::vector<Rectangle> idleRight = {
      {0, 256, 64, 128},   {64, 256, 64, 128},  {128, 256, 64, 128},
      {192, 256, 64, 128}, {256, 256, 64, 128}, {320, 256, 64, 128},
      {384, 256, 64, 128}, {448, 256, 64, 128}, {512, 256, 64, 128},
  };

  std::vector<Rectangle> idleUp = {
      {0, 384, 64, 128},   {64, 384, 64, 128},  {128, 384, 64, 128},
      {192, 384, 64, 128}, {256, 384, 64, 128}, {320, 384, 64, 128},
      {384, 384, 64, 128}, {448, 384, 64, 128}, {512, 384, 64, 128},

  };

  auto *anim = World.AddComponent<criogenio::AnimatedSprite>(
      entityId,
      "idle_down", // initial animation
      idleDown,    // frames
      0.10f,       // speed
      texture);

  anim->AddAnimation("idle_up", idleUp, 0.10f);
  anim->AddAnimation("idle_left", idleLeft, 0.10f);
  anim->AddAnimation("idle_right", idleRight, 0.10f);

  // TODO:(maraujo)
  //  That should come from a file, since I cahn edit it on editor

  // Add walking animation, should start after 64 pixels in y axis
  std::vector<Rectangle> walkDown = {
      {0, 512, 64, 128},   {64, 512, 64, 128},  {128, 512, 64, 128},
      {192, 512, 64, 128}, {256, 512, 64, 128}, {320, 512, 64, 128},
      {384, 512, 64, 128}, {448, 512, 64, 128}, {512, 512, 64, 128},
  };
  anim->AddAnimation("walk_down", walkDown, 0.1f);
  std::vector<Rectangle> walkLeft = {
      {0, 640, 64, 128},   {64, 640, 64, 128},  {128, 640, 64, 128},
      {192, 640, 64, 128}, {256, 640, 64, 128}, {320, 640, 64, 128},
      {384, 640, 64, 128}, {448, 640, 64, 128}, {512, 640, 64, 128},
  };
  anim->AddAnimation("walk_left", walkLeft, 0.1f);
  std::vector<Rectangle> walkRight = {
      {0, 768, 64, 128},   {64, 768, 64, 128},  {128, 768, 64, 128},
      {192, 768, 64, 128}, {256, 768, 64, 128}, {320, 768, 64, 128},
      {384, 768, 64, 128}, {448, 768, 64, 128}, {512, 768, 64, 128},
  };

  anim->AddAnimation("walk_right", walkRight, 0.1f);
  std::vector<Rectangle> walkUp = {
      {0, 896, 64, 128},   {64, 896, 64, 128},  {128, 896, 64, 128},
      {192, 896, 64, 128}, {256, 896, 64, 128}, {320, 896, 64, 128},
      {384, 896, 64, 128}, {448, 896, 64, 128}, {512, 896, 64, 128},
  };
  anim->AddAnimation("walk_up", walkUp, 0.1f);

  // TODO:inter dependent of animatin sprited component
  World.AddComponent<criogenio::AnimationState>(entityId);
  World.AddComponent<criogenio::Controller>(entityId, 200);

  static_assert(std::is_base_of_v<criogenio::ISystem, MySystem>);
  static_assert(!std::is_abstract_v<MySystem>);
  World.AddSystem<MySystem>(World);

  engine.Run();
  return 0;
}
