#pragma once

// ============================================================================
// MIGRATION EXAMPLE: Converting Legacy Systems to ECS
// ============================================================================
//
// This file demonstrates how to migrate from the legacy World-based systems
// to the new World2 ECS system. The API is mostly identical, but the internal
// performance characteristics are dramatically better.
//
// MIGRATION STRATEGY:
// 1. Minimal changes for most systems (just replace World with World2)
// 2. Optional optimization: use direct array iteration for hot loops
// 3. Add new systems using the ECS query patterns
//
// ============================================================================

#include "world2.h"

namespace criogenio::examples {

// ============================================================================
// BEFORE: Legacy MovementSystem with World
// ============================================================================

class MovementSystemLegacy {
public:
  World &world; // Old API
  MovementSystemLegacy(World &w) : world(w) {}

  void Update(float dt) {
    // Get entities with Controller component
    auto ids = world.GetEntitiesWith<Controller>();

    for (int id : ids) {
      auto &ctrl = world.GetComponent<Controller>(id);
      auto &tr = world.GetComponent<Transform>(id);
      auto &anim = world.GetComponent<AnimationState>(id);

      // ... movement logic
    }
  }
};

// ============================================================================
// AFTER: Same system with World2 (zero code changes needed!)
// ============================================================================

class MovementSystemECS {
public:
  World2 &world; // New API (same interface!)
  MovementSystemECS(World2 &w) : world(w) {}

  void Update(float dt) {
    // Exact same code, but now using sparse-set backend
    // Performance: O(k) where k = entities with Controller
    // (was O(n*m) with old system: n = total entities, m = components per
    // entity)
    auto ids = world.GetEntitiesWith<Controller>();

    for (int id : ids) {
      auto &ctrl = world.GetComponent<Controller>(id);
      auto &tr = world.GetComponent<Transform>(id);
      auto &anim = world.GetComponent<AnimationState>(id);

      // ... movement logic (unchanged)
    }
  }
};

// ============================================================================
// OPTIMIZATION 1: Direct Array Iteration (Cache-Friendly)
// ============================================================================

class MovementSystemOptimized {
public:
  World2 &world;
  MovementSystemOptimized(World2 &w) : world(w) {}

  void Update(float dt) {
    // For hot loops, iterate directly on the dense component array
    // This is significantly more cache-efficient than GetComponent() lookups

    auto &registry = ecs::Registry::instance();
    auto &transforms = registry.get_component_array<Transform>();

    // This transforms array is cache-contiguous
    // All transforms are sequential in memory -> cache hits!
    for (auto &transform : transforms) {
      // Can't directly get other components this way
      // So we still need to use query for multi-component systems
    }

    // But for single-component iteration, this is the fastest approach:
    for (auto &trans : transforms) {
      trans.x += 0.5f * dt; // Pure cache-efficient memory access
    }
  }
};

// ============================================================================
// OPTIMIZATION 2: Multi-Component Query (Still Very Fast)
// ============================================================================

class MovementSystemOptimized2 {
public:
  World2 &world;
  MovementSystemOptimized2(World2 &w) : world(w) {}

  void Update(float dt) {
    // Query for entities with BOTH Controller and Transform
    // Much faster than old system because it only iterates relevant entities
    auto entities = world.GetEntitiesWith<Controller, Transform>();

    for (auto entity_id : entities) {
      auto *ctrl = world.GetComponent<Controller>(entity_id);
      auto *trans = world.GetComponent<Transform>(entity_id);

      // Movement logic here
      if (ctrl && trans) {
        trans->x += ctrl->speed * dt;
      }
    }
  }
};

// ============================================================================
// NEW PATTERN: Component Arrays for Maximum Performance
// ============================================================================

class MovementSystemMaxPerformance {
public:
  World2 &world;
  MovementSystemMaxPerformance(World2 &w) : world(w) {}

  void Update(float dt) {
    auto &registry = ecs::Registry::instance();

    // For maximum performance, directly access the dense arrays
    // Use view() to get entities, then access components via pointers
    auto entities = registry.view<Controller, Transform>();

    for (auto entity_id : entities) {
      auto *ctrl = registry.get_component<Controller>(entity_id);
      auto *trans = registry.get_component<Transform>(entity_id);

      if (ctrl && trans) {
        // This is the fastest way to iterate with multiple components
        trans->x += ctrl->speed * dt;
      }
    }
  }
};

// ============================================================================
// NEW CAPABILITY: Easy Query Combinations
// ============================================================================

class DamageSystem {
public:
  World2 &world;
  DamageSystem(World2 &w) : world(w) {}

  void Update(float dt) {
    // Query: entities with BOTH Health AND Damage components
    // Old system would require complicated manual filtering
    auto entities = world.GetEntitiesWith<Health, Damage>();

    for (auto entity_id : entities) {
      auto *health = world.GetComponent<Health>(entity_id);
      auto *damage = world.GetComponent<Damage>(entity_id);

      if (health && damage) {
        health->current -= damage->amount * dt;
      }
    }
  }
};

// ============================================================================
// MIGRATION CHECKLIST
// ============================================================================
//
// To migrate a system from World to World2:
//
// [ ] Change: World& world -> World2& world
// [ ] Compile and test
// [ ] (Optional) Add multi-component queries where applicable
// [ ] (Optional) Use direct array iteration for hot loops
// [ ] Benchmark and profile
//
// Expected benefits:
// - ✅ Faster queries
// - ✅ Better cache locality
// - ✅ Easier multi-component filtering
// - ✅ Scales to 100k+ entities
//
// ============================================================================

} // namespace criogenio::examples
