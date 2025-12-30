# ECS Quick Reference Card

## Core Types

```cpp
// Type aliases
using EntityId = uint32_t;
using ComponentTypeId = uint32_t;

// Constants
constexpr EntityId NULL_ENTITY = std::numeric_limits<EntityId>::max();
constexpr EntityId FIRST_ENTITY_ID = 1;
```

---

## Creating & Managing Entities

```cpp
// Via World2 (recommended user API)
World2& world = /* get world */;
EntityId player = world.CreateEntity("player");
world.DeleteEntity(player);
bool exists = world.HasEntity(player);

// Via Registry (lower-level, more control)
auto& registry = ecs::Registry::instance();
EntityId enemy = registry.create_entity();
registry.destroy_entity(enemy);
bool alive = registry.has_entity(enemy);
```

---

## Adding & Removing Components

```cpp
World2& world = /* get world */;
EntityId entity = /* entity id */;

// Add component (can pass constructor args)
auto& transform = world.AddComponent<Transform>(entity, 10.0f, 20.0f);

// Get component (returns pointer, nullptr if not present)
auto* velocity = world.GetComponent<Velocity>(entity);
if (velocity) {
  velocity->x += 5.0f;
}

// Check if entity has component
if (world.HasComponent<Health>(entity)) {
  auto* health = world.GetComponent<Health>(entity);
  health->current -= 10.0f;
}

// Remove component
world.RemoveComponent<Velocity>(entity);
```

---

## Querying Entities

```cpp
World2& world = /* get world */;

// Single component query
auto entities = world.GetEntitiesWith<Transform>();
for (auto entity_id : entities) {
  auto* trans = world.GetComponent<Transform>(entity_id);
  trans->x += 1.0f;
}

// Multi-component query (entities with BOTH components)
auto moving_entities = world.GetEntitiesWith<Transform, Velocity>();
for (auto entity_id : moving_entities) {
  auto* trans = world.GetComponent<Transform>(entity_id);
  auto* vel = world.GetComponent<Velocity>(entity_id);
  trans->x += vel->x * dt;
}

// Up to 4 components
auto entities = world.GetEntitiesWith<A, B, C>();
auto entities = world.GetEntitiesWith<A, B, C, D>();
```

---

## Direct Component Array Iteration (Performance)

```cpp
auto& registry = ecs::Registry::instance();

// Get dense array of all components of type T
// This is cache-contiguous and fastest for iteration
auto& transforms = registry.get_component_array<Transform>();
for (auto& trans : transforms) {
  trans.x += 0.5f;  // Very fast! (CPU cache happy)
}

// Best for hot loops where you only care about one component type
```

---

## Component Type ID

```cpp
// Each component type gets a unique ID (per-type, static)
auto id1 = GetComponentTypeId<Transform>();
auto id2 = GetComponentTypeId<Transform>();  // Same ID
assert(id1 == id2);

auto id3 = GetComponentTypeId<Velocity>();   // Different ID
assert(id1 != id3);

// Thread-safe (uses static initialization)
```

---

## Serialization

```cpp
World2& world = /* get world */;

// Serialize all entities and components
SerializedWorld data = world.Serialize();

// Save to JSON or binary...
SaveToFile(data);

// Deserialize
SerializedWorld loaded_data = LoadFromFile();
world.Deserialize(loaded_data);  // Clears world first
```

---

## Creating a System

```cpp
#include "world2.h"

class HealthSystem : public ISystem {
public:
  World2& world;
  
  HealthSystem(World2& w) : world(w) {}

  void Update(float dt) override {
    // Query for entities with Health and Damage components
    auto entities = world.GetEntitiesWith<Health, Damage>();
    
    for (auto entity_id : entities) {
      auto* health = world.GetComponent<Health>(entity_id);
      auto* damage = world.GetComponent<Damage>(entity_id);
      
      if (health && damage) {
        health->current -= damage->amount * dt;
        
        if (health->current <= 0.0f) {
          world.DeleteEntity(entity_id);
        }
      }
    }
  }

  void Render(Renderer&) override {}
};

// Register system in world
world.AddSystem<HealthSystem>();
```

---

## Performance Tips

### ✅ DO

```cpp
// 1. Use multi-component queries for filtering
auto entities = world.GetEntitiesWith<Active, Visible, Transform>();

// 2. Use direct array iteration for hot loops
auto& transforms = registry.get_component_array<Transform>();
for (auto& trans : transforms) {
  // ... fast iteration
}

// 3. Cache component pointers across frames if needed
auto* player_transform = world.GetComponent<Transform>(player_id);

// 4. Delete entities when done with them
world.DeleteEntity(unused_entity);
```

### ❌ DON'T

```cpp
// 1. Don't repeatedly call GetComponent in tight loops
// (bad) 
for (int i = 0; i < 1000; i++) {
  auto* trans = world.GetComponent<Transform>(entity);
  trans->x += 1.0f;  // Repeated lookups
}

// (good)
auto* trans = world.GetComponent<Transform>(entity);
for (int i = 0; i < 1000; i++) {
  trans->x += 1.0f;  // Single lookup
}

// 2. Don't add/remove components during iteration
auto entities = world.GetEntitiesWith<Transform>();
for (auto entity_id : entities) {
  world.AddComponent<Velocity>(entity_id);  // DANGER: modifying during iteration
}

// 3. Don't assume component existence without checking
auto* vel = world.GetComponent<Velocity>(entity);
vel->x += 1.0f;  // CRASH if component doesn't exist!

// (safe)
auto* vel = world.GetComponent<Velocity>(entity);
if (vel) {
  vel->x += 1.0f;
}
```

---

## Common Patterns

### Pattern 1: Component-Based Movement

```cpp
void UpdateMovement(World2& world, float dt) {
  auto entities = world.GetEntitiesWith<Transform, Velocity>();
  
  for (auto entity_id : entities) {
    auto* trans = world.GetComponent<Transform>(entity_id);
    auto* vel = world.GetComponent<Velocity>(entity_id);
    
    trans->x += vel->x * dt;
    trans->y += vel->y * dt;
  }
}
```

### Pattern 2: Conditional Component Operations

```cpp
void SpawnEffect(World2& world, EntityId entity) {
  if (!world.HasComponent<Health>(entity)) {
    return;  // Only spawn if entity has health
  }
  
  auto* health = world.GetComponent<Health>(entity);
  if (health->current < health->max * 0.25f) {
    world.AddComponent<DamagedEffect>(entity);
  }
}
```

### Pattern 3: Cleanup Dead Entities

```cpp
void CleanupDeadEntities(World2& world) {
  auto entities = world.GetEntitiesWith<Health>();
  
  // Collect IDs to delete (can't delete during iteration)
  std::vector<EntityId> to_delete;
  for (auto entity_id : entities) {
    auto* health = world.GetComponent<Health>(entity_id);
    if (health->current <= 0.0f) {
      to_delete.push_back(entity_id);
    }
  }
  
  // Now delete outside the iteration
  for (auto entity_id : to_delete) {
    world.DeleteEntity(entity_id);
  }
}
```

### Pattern 4: Single Entity Lookup

```cpp
void UpdatePlayer(World2& world, EntityId player_id, float dt) {
  auto* trans = world.GetComponent<Transform>(player_id);
  auto* ctrl = world.GetComponent<Controller>(player_id);
  auto* anim = world.GetComponent<AnimatedSprite>(player_id);
  
  if (trans && ctrl && anim) {
    // Update logic
    trans->x += ctrl->input_x * ctrl->speed * dt;
    trans->y += ctrl->input_y * ctrl->speed * dt;
    anim->Update(dt);
  }
}
```

---

## Debugging Tips

```cpp
// Check if entity exists
if (!world.HasEntity(entity_id)) {
  std::cerr << "Entity " << entity_id << " doesn't exist!" << std::endl;
}

// Check which components an entity has
auto* trans = world.GetComponent<Transform>(entity_id);
auto* vel = world.GetComponent<Velocity>(entity_id);
auto* health = world.GetComponent<Health>(entity_id);

if (trans) std::cout << "Has Transform" << std::endl;
if (vel) std::cout << "Has Velocity" << std::endl;
if (health) std::cout << "Has Health" << std::endl;

// Count entities with specific components
auto count = world.GetEntitiesWith<Transform>().size();
std::cout << "Entities with Transform: " << count << std::endl;
```

---

## Related Documentation

- **Full Guide**: See [ECS_REFACTOR.md](../ECS_REFACTOR.md)
- **Migration Examples**: See [ecs_migration_examples.h](../engine/include/ecs_migration_examples.h)
- **Implementation**: See [ecs_core.h](../engine/include/ecs_core.h) and [ecs_registry.h](../engine/include/ecs_registry.h)

---

## Cheat Sheet Reference

| Operation | Code | Performance |
|-----------|------|-------------|
| Create entity | `registry.create_entity()` | O(1) |
| Destroy entity | `registry.destroy_entity(id)` | O(1) |
| Add component | `registry.add_component<T>(id, data)` | O(1) |
| Get component | `registry.get_component<T>(id)` | O(1) |
| Remove component | `registry.remove_component<T>(id)` | O(log n) |
| Query 1 component | `registry.view<T>()` | O(k) |
| Query 2 components | `registry.view<T, U>()` | O(k) |
| Iterate all T | For loop on `registry.get_component_array<T>()` | O(k) cache-friendly |

Where: `k` = entities with specific components (typically much < n = total entities)

