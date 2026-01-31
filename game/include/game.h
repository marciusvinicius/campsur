#pragma once

#include "engine.h"
#include "network/net_messages.h"
#include "render.h"
#include "systems.h"
#include "world.h"

static constexpr int InitialWidth = 1200;
static constexpr int InitialHeight = 800;
static constexpr uint16_t DefaultNetPort = 7777;

class MySystem : public criogenio::ISystem {
public:
  criogenio::World &world;
  MySystem(criogenio::World &w) : world(w) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

/** Game engine that sends client input each frame when in client mode. */
class GameEngine : public criogenio::Engine {
public:
  using criogenio::Engine::Engine;
  void OnFrame(float dt) override;
};
