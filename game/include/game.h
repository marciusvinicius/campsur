#pragma once

#include "render.h"
#include "systems.h"
#include "world.h"

static constexpr int InitialWidth = 1200;
static constexpr int InitialHeight = 800;

class MySystem : public criogenio::ISystem {
public:
  criogenio::World &world;
  MySystem(criogenio::World &w) : world(w) {};
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};
