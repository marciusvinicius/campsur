#pragma once
#include "ecs_core.h"
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace criogenio::ecs {

// ============================================================================
// Component Storage Interface
// ============================================================================

class IComponentStorage {
public:
  virtual ~IComponentStorage() = default;
  virtual void remove(EntityId entity) = 0;
  virtual void clear() = 0;
};

// ============================================================================
// Typed Component Storage
// ============================================================================

template <typename T> class ComponentStorage : public IComponentStorage {
public:
  T &add(EntityId entity, const T &component) {
    return set.add(entity, component);
  }

  const T *get(EntityId entity) const { return set.get(entity); }

  T *get(EntityId entity) { return set.get(entity); }

  bool contains(EntityId entity) const { return set.contains(entity); }

  void remove(EntityId entity) override { set.remove(entity); }

  void clear() override { set.clear(); }

  const std::vector<EntityId> &get_entities() const {
    return set.get_entities();
  }

  const std::vector<T> &get_components() const { return set.get_components(); }

  std::vector<T> &get_components() { return set.get_components(); }

private:
  SparseSet<T> set;
};

// ============================================================================
// Query: Find entities matching a component pattern
// ============================================================================

class Query {
public:
  explicit Query(const Signature &required_sig) : required_sig(required_sig) {}

  std::vector<EntityId> build() {
    std::vector<EntityId> results;
    const auto &all_entities = EntityManager::instance().get_alive_entities();

    for (EntityId entity : all_entities) {
      const auto &sig = EntityManager::instance().get_signature_const(entity);
      if (sig.matches(required_sig)) {
        results.push_back(entity);
      }
    }

    return results;
  }

private:
  Signature required_sig;
};

// ============================================================================
// Registry: Main ECS container
// ============================================================================

class Registry {
public:
  static Registry &instance() {
    static Registry reg;
    return reg;
  }

  // ========================================================================
  // Entity Management
  // ========================================================================

  EntityId create_entity() {
    EntityId id = EntityManager::instance().create();
    return id;
  }

  void destroy_entity(EntityId entity) {
    // Remove all components for this entity
    for (auto &[type_id, storage] : component_storage) {
      storage->remove(entity);
    }

    EntityManager::instance().destroy(entity);
  }

  bool has_entity(EntityId entity) const {
    return EntityManager::instance().alive(entity);
  }

  const std::vector<EntityId> &get_entities() const {
    return EntityManager::instance().get_alive_entities();
  }

  // ========================================================================
  // Component Management
  // ========================================================================

  template <typename T> T &add_component(EntityId entity, const T &component) {
    ComponentTypeId type_id = ComponentTypeRegistry::GetTypeId<T>();

    // Get or create storage
    if (component_storage.find(type_id) == component_storage.end()) {
      component_storage[type_id] = std::make_unique<ComponentStorage<T>>();
    }

    auto storage =
        static_cast<ComponentStorage<T> *>(component_storage[type_id].get());

    // Add component
    T &comp_ref = storage->add(entity, component);

    // Update entity signature
    auto &sig = EntityManager::instance().get_signature(entity);
    sig.add(type_id);

    return comp_ref;
  }

  template <typename T> T *get_component(EntityId entity) {
    ComponentTypeId type_id = ComponentTypeRegistry::GetTypeId<T>();

    auto it = component_storage.find(type_id);
    if (it == component_storage.end()) {
      return nullptr;
    }

    auto storage = static_cast<ComponentStorage<T> *>(it->second.get());
    return storage->get(entity);
  }

  template <typename T> const T *get_component(EntityId entity) const {
    ComponentTypeId type_id = ComponentTypeRegistry::GetTypeId<T>();

    auto it = component_storage.find(type_id);
    if (it == component_storage.end()) {
      return nullptr;
    }

    const auto storage =
        static_cast<const ComponentStorage<T> *>(it->second.get());
    return storage->get(entity);
  }

  template <typename T> bool has_component(EntityId entity) const {
    ComponentTypeId type_id = ComponentTypeRegistry::GetTypeId<T>();

    auto it = component_storage.find(type_id);
    if (it == component_storage.end()) {
      return false;
    }

    const auto storage =
        static_cast<const ComponentStorage<T> *>(it->second.get());
    return storage->contains(entity);
  }

  template <typename T> void remove_component(EntityId entity) {
    ComponentTypeId type_id = ComponentTypeRegistry::GetTypeId<T>();

    auto it = component_storage.find(type_id);
    if (it == component_storage.end()) {
      return;
    }

    auto storage = static_cast<ComponentStorage<T> *>(it->second.get());
    storage->remove(entity);

    // Update entity signature
    EntityManager::instance().get_signature(entity).remove(type_id);
  }

  // ========================================================================
  // Queries
  // ========================================================================

  template <typename T> std::vector<EntityId> view() {
    Signature sig;
    sig.add(ComponentTypeRegistry::GetTypeId<T>());

    Query query(sig);
    return query.build();
  }

  template <typename T, typename U> std::vector<EntityId> view() {
    Signature sig;
    sig.add(ComponentTypeRegistry::GetTypeId<T>());
    sig.add(ComponentTypeRegistry::GetTypeId<U>());

    Query query(sig);
    return query.build();
  }

  template <typename T, typename U, typename V> std::vector<EntityId> view() {
    Signature sig;
    sig.add(ComponentTypeRegistry::GetTypeId<T>());
    sig.add(ComponentTypeRegistry::GetTypeId<U>());
    sig.add(ComponentTypeRegistry::GetTypeId<V>());

    Query query(sig);
    return query.build();
  }

  template <typename T, typename U, typename V, typename W>
  std::vector<EntityId> view() {
    Signature sig;
    sig.add(ComponentTypeRegistry::GetTypeId<T>());
    sig.add(ComponentTypeRegistry::GetTypeId<U>());
    sig.add(ComponentTypeRegistry::GetTypeId<V>());
    sig.add(ComponentTypeRegistry::GetTypeId<W>());

    Query query(sig);
    return query.build();
  }

  // ========================================================================
  // Direct Component Storage Access (for iteration)
  // ========================================================================

  template <typename T> const std::vector<T> &get_component_array() const {
    ComponentTypeId type_id = ComponentTypeRegistry::GetTypeId<T>();

    auto it = component_storage.find(type_id);
    if (it == component_storage.end()) {
      static std::vector<T> empty;
      return empty;
    }

    const auto storage =
        static_cast<const ComponentStorage<T> *>(it->second.get());
    return storage->get_components();
  }

  template <typename T> std::vector<T> &get_component_array() {
    ComponentTypeId type_id = ComponentTypeRegistry::GetTypeId<T>();

    auto it = component_storage.find(type_id);
    if (it == component_storage.end()) {
      static std::vector<T> empty;
      return empty;
    }

    auto storage = static_cast<ComponentStorage<T> *>(it->second.get());
    return storage->get_components();
  }

  // ========================================================================
  // Utility
  // ========================================================================

  void clear() {
    component_storage.clear();
    EntityManager::instance().clear();
  }

private:
  std::unordered_map<ComponentTypeId, std::unique_ptr<IComponentStorage>>
      component_storage;
};

} // namespace criogenio::ecs
