#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace criogenio::ecs {

// ============================================================================
// Types
// ============================================================================

using EntityId = uint32_t;
using ComponentTypeId = uint32_t;

constexpr EntityId NULL_ENTITY = std::numeric_limits<EntityId>::max();
constexpr EntityId FIRST_ENTITY_ID = 1;

// ============================================================================
// Component Type Registry
// ============================================================================

class ComponentTypeRegistry {
public:
  static ComponentTypeRegistry &instance() {
    static ComponentTypeRegistry reg;
    return reg;
  }

  template <typename T> static ComponentTypeId GetTypeId() {
    static ComponentTypeId id = instance().next_id++;
    return id;
  }

private:
  ComponentTypeId next_id = 0;
};

// ============================================================================
// SparseSet: Core data structure for efficient component storage
// ============================================================================

template <typename T> class SparseSet {
public:
  // Add or update a component for an entity
  T &add(EntityId entity, const T &component) {
    if (entity >= sparse.size()) {
      sparse.resize(entity + 1, NULL_INDEX);
    }

    if (sparse[entity] != NULL_INDEX) {
      // Already exists, update it
      dense[sparse[entity]] = component;
    } else {
      // New component
      sparse[entity] = dense.size();
      dense.push_back(component);
      entities.push_back(entity);
    }

    return dense[sparse[entity]];
  }

  // Get a component for an entity (const)
  const T *get(EntityId entity) const {
    if (entity >= sparse.size() || sparse[entity] == NULL_INDEX) {
      return nullptr;
    }
    return &dense[sparse[entity]];
  }

  // Get a component for an entity (mutable)
  T *get(EntityId entity) {
    if (entity >= sparse.size() || sparse[entity] == NULL_INDEX) {
      return nullptr;
    }
    return &dense[sparse[entity]];
  }

  // Check if entity has this component
  bool contains(EntityId entity) const {
    return entity < sparse.size() && sparse[entity] != NULL_INDEX;
  }

  // Remove a component from an entity
  void remove(EntityId entity) {
    if (entity >= sparse.size() || sparse[entity] == NULL_INDEX) {
      return;
    }

    uint32_t idx = sparse[entity];
    uint32_t last_idx = dense.size() - 1;

    if (idx != last_idx) {
      // Swap with last element
      EntityId last_entity = entities[last_idx];
      dense[idx] = dense[last_idx];
      entities[idx] = last_entity;
      sparse[last_entity] = idx;
    }

    // Remove last element
    dense.pop_back();
    entities.pop_back();
    sparse[entity] = NULL_INDEX;
  }

  // Get all entities that have this component
  const std::vector<EntityId> &get_entities() const { return entities; }

  // Clear all components
  void clear() {
    sparse.clear();
    dense.clear();
    entities.clear();
  }

  // Get number of components
  size_t size() const { return dense.size(); }

  // Direct access to dense array (for iteration)
  const std::vector<T> &get_components() const { return dense; }

  std::vector<T> &get_components() { return dense; }

private:
  static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

  std::vector<uint32_t> sparse;   // entity -> index in dense
  std::vector<T> dense;           // actual component data (cache-friendly)
  std::vector<EntityId> entities; // which entities are in dense
};

// ============================================================================
// Signature: Bitset-like tracking of component types per entity
// ============================================================================

class Signature {
public:
  void add(ComponentTypeId type_id) {
    if (type_id >= bits.size()) {
      bits.resize(type_id + 1, false);
    }
    bits[type_id] = true;
  }

  void remove(ComponentTypeId type_id) {
    if (type_id < bits.size()) {
      bits[type_id] = false;
    }
  }

  bool contains(const Signature &other) const {
    if (other.bits.size() > bits.size()) {
      return false;
    }
    for (size_t i = 0; i < other.bits.size(); ++i) {
      if (other.bits[i] && !bits[i]) {
        return false;
      }
    }
    return true;
  }

  bool matches(const Signature &required) const { return contains(required); }

  bool operator==(const Signature &other) const { return bits == other.bits; }

  void clear() { bits.assign(bits.size(), false); }

  const std::vector<bool> &get_bits() const { return bits; }

private:
  std::vector<bool> bits;
};

// ============================================================================
// Entity Manager
// ============================================================================

class EntityManager {
public:
  static EntityManager &instance() {
    static EntityManager manager;
    return manager;
  }

  EntityId create() {
    EntityId id = next_id++;
    signatures[id] = Signature();
    alive_entities.push_back(id);
    return id;
  }

  void destroy(EntityId entity) {
    if (signatures.find(entity) != signatures.end()) {
      signatures.erase(entity);
    }
    auto it = std::find(alive_entities.begin(), alive_entities.end(), entity);
    if (it != alive_entities.end()) {
      alive_entities.erase(it);
    }
  }

  bool alive(EntityId entity) const {
    return signatures.find(entity) != signatures.end();
  }

  const std::vector<EntityId> &get_alive_entities() const {
    return alive_entities;
  }

  void set_signature(EntityId entity, const Signature &signature) {
    signatures[entity] = signature;
  }

  Signature &get_signature(EntityId entity) {
    static Signature empty;
    auto it = signatures.find(entity);
    if (it != signatures.end()) {
      return it->second;
    }
    return empty;
  }

  const Signature &get_signature_const(EntityId entity) const {
    static Signature empty;
    auto it = signatures.find(entity);
    if (it != signatures.end()) {
      return it->second;
    }
    return empty;
  }

  void clear() {
    signatures.clear();
    alive_entities.clear();
    next_id = FIRST_ENTITY_ID;
  }

private:
  EntityId next_id = FIRST_ENTITY_ID;
  std::unordered_map<EntityId, Signature> signatures;
  std::vector<EntityId> alive_entities;
};

} // namespace criogenio::ecs
