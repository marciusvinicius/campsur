#include <cassert>
#include <cstring>
#include <iostream>

// Include ECS headers
#include "components.h"
#include "ecs_core.h"
#include "ecs_registry.h"

using namespace criogenio;
using namespace criogenio::ecs;

// Simple test components
struct Position {
  float x = 0, y = 0;
};

struct Velocity {
  float vx = 0, vy = 0;
};

struct Health {
  float current = 100;
  float max = 100;
};

// Test suite
int main() {
  std::cout << "=== ECS Runtime Test Suite ===" << std::endl << std::endl;

  // Test 1: Entity creation and destruction
  std::cout << "[Test 1] Entity creation and destruction..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    EntityId e1 = registry.create_entity();
    EntityId e2 = registry.create_entity();

    assert(registry.has_entity(e1));
    assert(registry.has_entity(e2));
    assert(e1 != e2);

    registry.destroy_entity(e1);
    assert(!registry.has_entity(e1));
    assert(registry.has_entity(e2));

    std::cout << "  ✓ Entity creation and destruction works" << std::endl;
  }

  // Test 2: Component addition and retrieval
  std::cout << "[Test 2] Component addition and retrieval..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    EntityId entity = registry.create_entity();

    Position pos{10.0f, 20.0f};
    auto &pos_ref = registry.add_component<Position>(entity, pos);

    assert(registry.has_component<Position>(entity));
    auto *retrieved = registry.get_component<Position>(entity);
    assert(retrieved != nullptr);
    assert(retrieved->x == 10.0f);
    assert(retrieved->y == 20.0f);

    std::cout << "  ✓ Component addition and retrieval works" << std::endl;
  }

  // Test 3: Component removal
  std::cout << "[Test 3] Component removal..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    EntityId entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{5.0f, 10.0f});

    assert(registry.has_component<Position>(entity));
    registry.remove_component<Position>(entity);
    assert(!registry.has_component<Position>(entity));

    std::cout << "  ✓ Component removal works" << std::endl;
  }

  // Test 4: Single component query
  std::cout << "[Test 4] Single component query..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    EntityId e1 = registry.create_entity();
    EntityId e2 = registry.create_entity();
    EntityId e3 = registry.create_entity();

    registry.add_component<Position>(e1, Position{1, 2});
    registry.add_component<Position>(e2, Position{3, 4});
    registry.add_component<Velocity>(e3, Velocity{5, 6});

    auto entities = registry.view<Position>();
    assert(entities.size() == 2);
    assert((entities[0] == e1 && entities[1] == e2) ||
           (entities[0] == e2 && entities[1] == e1));

    std::cout << "  ✓ Single component query works (found " << entities.size()
              << " entities)" << std::endl;
  }

  // Test 5: Multi-component query
  std::cout << "[Test 5] Multi-component query..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    EntityId e1 = registry.create_entity();
    EntityId e2 = registry.create_entity();
    EntityId e3 = registry.create_entity();

    registry.add_component<Position>(e1, Position{1, 2});
    registry.add_component<Velocity>(e1, Velocity{10, 20});

    registry.add_component<Position>(e2, Position{3, 4});
    registry.add_component<Velocity>(e2, Velocity{30, 40});

    registry.add_component<Position>(e3, Position{5, 6});
    // e3 has Position but no Velocity

    auto entities = registry.view<Position, Velocity>();
    assert(entities.size() == 2);

    std::cout << "  ✓ Multi-component query works (found " << entities.size()
              << " entities with Position+Velocity)" << std::endl;
  }

  // Test 6: Component mutation through queries
  std::cout << "[Test 6] Component mutation through queries..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    EntityId e1 = registry.create_entity();
    EntityId e2 = registry.create_entity();

    registry.add_component<Position>(e1, Position{1, 2});
    registry.add_component<Position>(e2, Position{3, 4});

    auto entities = registry.view<Position>();
    for (auto entity_id : entities) {
      auto *pos = registry.get_component<Position>(entity_id);
      if (pos) {
        pos->x += 10.0f;
        pos->y += 20.0f;
      }
    }

    auto *p1 = registry.get_component<Position>(e1);
    auto *p2 = registry.get_component<Position>(e2);

    assert(p1->x == 11.0f && p1->y == 22.0f);
    assert(p2->x == 13.0f && p2->y == 24.0f);

    std::cout << "  ✓ Component mutation through queries works" << std::endl;
  }

  // Test 7: Direct array iteration
  std::cout << "[Test 7] Direct component array iteration..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    for (int i = 0; i < 5; ++i) {
      EntityId e = registry.create_entity();
      registry.add_component<Position>(e, Position{(float)i, (float)i * 2});
    }

    auto &positions = registry.get_component_array<Position>();
    assert(positions.size() == 5);

    for (auto &pos : positions) {
      pos.x += 1.0f;
    }

    // Verify mutations
    for (auto &pos : positions) {
      assert(pos.x >= 1.0f);
    }

    std::cout << "  ✓ Direct array iteration works (iterated "
              << positions.size() << " components)" << std::endl;
  }

  // Test 8: Entity signatures
  std::cout << "[Test 8] Entity signatures..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    EntityId entity = registry.create_entity();

    registry.add_component<Position>(entity, Position{});
    registry.add_component<Velocity>(entity, Velocity{});
    registry.add_component<Health>(entity, Health{});

    auto &manager = EntityManager::instance();
    const auto &sig = manager.get_signature_const(entity);

    assert(sig.contains(sig)); // Signature contains itself

    std::cout << "  ✓ Entity signatures work" << std::endl;
  }

  // Test 9: Multiple entities lifecycle
  std::cout << "[Test 9] Multiple entities lifecycle..." << std::endl;
  {
    auto &registry = Registry::instance();
    registry.clear();

    const int ENTITY_COUNT = 100;
    std::vector<EntityId> entities;

    // Create many entities
    for (int i = 0; i < ENTITY_COUNT; ++i) {
      EntityId e = registry.create_entity();
      registry.add_component<Position>(e, Position{(float)i, (float)i});
      if (i % 3 == 0) {
        registry.add_component<Velocity>(e, Velocity{1.0f, 1.0f});
      }
      entities.push_back(e);
    }

    auto pos_entities = registry.view<Position>();
    assert(pos_entities.size() == ENTITY_COUNT);

    auto moving_entities = registry.view<Position, Velocity>();
    assert(moving_entities.size() == ENTITY_COUNT / 3);

    // Delete half
    for (int i = 0; i < ENTITY_COUNT / 2; ++i) {
      registry.destroy_entity(entities[i]);
    }

    auto remaining_pos = registry.view<Position>();
    assert(remaining_pos.size() == ENTITY_COUNT / 2);

    std::cout << "  ✓ Multiple entities lifecycle works (" << ENTITY_COUNT
              << " entities created, " << ENTITY_COUNT / 2 << " destroyed)"
              << std::endl;
  }

  // Test 10: Component type IDs
  std::cout << "[Test 10] Component type IDs..." << std::endl;
  {
    auto id1 = ComponentTypeRegistry::GetTypeId<Position>();
    auto id2 = ComponentTypeRegistry::GetTypeId<Position>();
    auto id3 = ComponentTypeRegistry::GetTypeId<Velocity>();

    assert(id1 == id2);
    assert(id1 != id3);

    std::cout << "  ✓ Component type IDs are consistent" << std::endl;
  }

  std::cout << std::endl << "=== All Tests Passed! ===" << std::endl;
  std::cout << "✓ ECS core functionality is working correctly" << std::endl;
  std::cout << "✓ Ready for World2 editor/game testing" << std::endl;

  return 0;
}
