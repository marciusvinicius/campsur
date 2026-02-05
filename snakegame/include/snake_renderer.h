#pragma once
#include "components.h"
#include "graphics_types.h"
#include "systems.h"
#include "world.h"
#include <iostream>

class SnakeRenderSystem : public criogenio::ISystem {
public:
  SnakeRenderSystem(criogenio::World &w) : world(w) {
    std::cout << "SnakeRenderSystem initialized" << std::endl;
  }

  void Update(float) override {}

  void Render(criogenio::Renderer &renderer) override {
    auto ids = world.GetEntitiesWith<criogenio::Transform>();
    std::cout << "SnakeRenderSystem rendering " << ids.size()
              << " entities with Transform" << std::endl;

    for (auto id : ids) {
      auto *tr = world.GetComponent<criogenio::Transform>(id);
      if (!tr)
        continue;

      // Color by name when present
      if (world.HasComponent<criogenio::Name>(id)) {
        auto *n = world.GetComponent<criogenio::Name>(id);
        if (n && n->name == "Food") {
          std::cout << "  Drawing food at (" << tr->x << ", " << tr->y << ")"
                    << std::endl;
          renderer.DrawRect(tr->x, tr->y, 20, 20, criogenio::Colors::Green);
          continue;
        }
        if (n && n->name == "SnakePart") {
          std::cout << "  Drawing snake at (" << tr->x << ", " << tr->y << ")"
                    << std::endl;
          renderer.DrawRect(tr->x, tr->y, 20, 20, criogenio::Colors::Yellow);
          continue;
        }
      }

      // Default
      renderer.DrawRect(tr->x, tr->y, 20, 20, criogenio::Colors::White);
    }
  }

private:
  criogenio::World &world;
};
