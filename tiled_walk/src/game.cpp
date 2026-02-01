#include "game.h"
#include "raylib.h"

namespace tiled {

void TileMapSystem::Update(float /*dt*/) {}

void TileMapSystem::Render(criogenio::Renderer &renderer) {
  const int w = MapWidthTiles;
  const int h = MapHeightTiles;
  const int ts = TileSize;
  const Color a = {0x44, 0x88, 0x44, 255};  // grass green
  const Color b = {0x55, 0x99, 0x55, 255};  // grass light
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      bool alt = (x + y) % 2 == 0;
      renderer.DrawRect(static_cast<float>(x * ts), static_cast<float>(y * ts),
                        static_cast<float>(ts), static_cast<float>(ts),
                        alt ? a : b);
    }
  }
  // Thin grid lines
  const Color line = {0, 0, 0, 48};
  for (int x = 0; x <= w; ++x)
    renderer.DrawRect(static_cast<float>(x * ts), 0, 1, static_cast<float>(h * ts), line);
  for (int y = 0; y <= h; ++y)
    renderer.DrawRect(0, static_cast<float>(y * ts), static_cast<float>(w * ts), 1, line);
}

void VelocityMovementSystem::Update(float dt) {
  auto ids = world.GetEntitiesWith<criogenio::Controller, criogenio::Transform>();
  for (criogenio::ecs::EntityId id : ids) {
    auto *ctrl = world.GetComponent<criogenio::Controller>(id);
    auto *tr = world.GetComponent<criogenio::Transform>(id);
    if (!ctrl || !tr) continue;
    tr->x += ctrl->velocity.x * PlayerMoveSpeed * dt;
    tr->y += ctrl->velocity.y * PlayerMoveSpeed * dt;
    // Clamp to map bounds
    float maxX = (MapWidthTiles - 1) * static_cast<float>(TileSize);
    float maxY = (MapHeightTiles - 1) * static_cast<float>(TileSize);
    if (tr->x < 0) tr->x = 0;
    if (tr->x > maxX) tr->x = maxX;
    if (tr->y < 0) tr->y = 0;
    if (tr->y > maxY) tr->y = maxY;
  }
}

void VelocityMovementSystem::Render(criogenio::Renderer &) {}

void PlayerRenderSystem::Update(float /*dt*/) {}

void PlayerRenderSystem::Render(criogenio::Renderer &renderer) {
  // Query NetReplicated + Transform so server draws all players (server + clients); clients already have both from snapshots
  auto ids = world.GetEntitiesWith<criogenio::NetReplicated, criogenio::Transform>();
  static const Color kPlayerColors[] = {
    {0x22, 0x66, 0xdd, 255},  // blue
    {0xdd, 0x44, 0x44, 255},  // red
    {0x44, 0xbb, 0x44, 255},  // green
    {0xee, 0xcc, 0x22, 255},  // yellow
    {0xdd, 0x44, 0xaa, 255},  // magenta
    {0xff, 0x88, 0x22, 255},  // orange
  };
  constexpr int kNumColors = sizeof(kPlayerColors) / sizeof(kPlayerColors[0]);
  for (criogenio::ecs::EntityId id : ids) {
    auto *tr = world.GetComponent<criogenio::Transform>(id);
    if (!tr) continue;
    // Color from ReplicatedNetId when present (connection order: 0=blue, 1=red, ...); else blue
    int colorIndex = 0;
    if (auto *netIdComp = world.GetComponent<criogenio::ReplicatedNetId>(id))
      colorIndex = static_cast<int>(netIdComp->id) % kNumColors;
    if (colorIndex < 0) colorIndex = 0;
    renderer.DrawCircle(tr->x + TileSize / 2.f, tr->y + TileSize / 2.f,
                        PlayerRadius, kPlayerColors[colorIndex]);
  }
}

void GameEngine::OnFrame(float /*dt*/) {
  criogenio::PlayerInput input = {};
  if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) input.move_x += 1.f;
  if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) input.move_x -= 1.f;
  if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) input.move_y -= 1.f;
  if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) input.move_y += 1.f;

  if (GetNetworkMode() == criogenio::NetworkMode::Client) {
    SendInputAsClient(input);
  } else if (GetNetworkMode() == criogenio::NetworkMode::Server) {
    SetServerPlayerInput(input);
  }
}

}  // namespace tiled
