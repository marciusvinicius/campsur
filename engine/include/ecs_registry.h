#pragma once
#include "ecs_core.h"
#include <functional>
#include <iostream>
#include <memory>
#include <string>
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

  void build_into(std::vector<EntityId> &results) {
    results.clear();
    const auto &all_entities = EntityManager::instance().get_alive_entities();
    results.reserve(all_entities.size());

    for (EntityId entity : all_entities) {
      const auto &sig = EntityManager::instance().get_signature_const(entity);
      if (sig.matches(required_sig)) {
        results.push_back(entity);
      }
    }
  }

  std::vector<EntityId> build() {
    std::vector<EntityId> results;
    build_into(results);
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
    mark_mutated();
    return id;
  }

  void destroy_entity(EntityId entity) {
    // Remove all components for this entity
    for (auto &[type_id, storage] : component_storage) {
      storage->remove(entity);
    }

    EntityManager::instance().destroy(entity);
    mark_mutated();
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
    mark_mutated();

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
    mark_mutated();
  }

  // ========================================================================
  // Queries
  // ========================================================================

  template <typename T> std::vector<EntityId> view() {
    Signature sig;
    const ComponentTypeId t = ComponentTypeRegistry::GetTypeId<T>();
    sig.add(t);
    return get_view_cached(sig, make_query_key({t}));
  }

  template <typename T> void view(std::vector<EntityId> &out) {
    Signature sig;
    const ComponentTypeId t = ComponentTypeRegistry::GetTypeId<T>();
    sig.add(t);
    get_view_cached_into(sig, make_query_key({t}), out);
  }

  template <typename T, typename U> std::vector<EntityId> view() {
    Signature sig;
    const ComponentTypeId t = ComponentTypeRegistry::GetTypeId<T>();
    const ComponentTypeId u = ComponentTypeRegistry::GetTypeId<U>();
    sig.add(t);
    sig.add(u);
    return get_view_cached(sig, make_query_key({t, u}));
  }

  template <typename T, typename U> void view(std::vector<EntityId> &out) {
    Signature sig;
    const ComponentTypeId t = ComponentTypeRegistry::GetTypeId<T>();
    const ComponentTypeId u = ComponentTypeRegistry::GetTypeId<U>();
    sig.add(t);
    sig.add(u);
    get_view_cached_into(sig, make_query_key({t, u}), out);
  }

  template <typename T, typename U, typename V> std::vector<EntityId> view() {
    Signature sig;
    const ComponentTypeId t = ComponentTypeRegistry::GetTypeId<T>();
    const ComponentTypeId u = ComponentTypeRegistry::GetTypeId<U>();
    const ComponentTypeId v = ComponentTypeRegistry::GetTypeId<V>();
    sig.add(t);
    sig.add(u);
    sig.add(v);
    return get_view_cached(sig, make_query_key({t, u, v}));
  }

  template <typename T, typename U, typename V>
  void view(std::vector<EntityId> &out) {
    Signature sig;
    const ComponentTypeId t = ComponentTypeRegistry::GetTypeId<T>();
    const ComponentTypeId u = ComponentTypeRegistry::GetTypeId<U>();
    const ComponentTypeId v = ComponentTypeRegistry::GetTypeId<V>();
    sig.add(t);
    sig.add(u);
    sig.add(v);
    get_view_cached_into(sig, make_query_key({t, u, v}), out);
  }

  template <typename T, typename U, typename V, typename W>
  std::vector<EntityId> view() {
    Signature sig;
    const ComponentTypeId t = ComponentTypeRegistry::GetTypeId<T>();
    const ComponentTypeId u = ComponentTypeRegistry::GetTypeId<U>();
    const ComponentTypeId v = ComponentTypeRegistry::GetTypeId<V>();
    const ComponentTypeId w = ComponentTypeRegistry::GetTypeId<W>();
    sig.add(t);
    sig.add(u);
    sig.add(v);
    sig.add(w);
    return get_view_cached(sig, make_query_key({t, u, v, w}));
  }

  template <typename T, typename U, typename V, typename W>
  void view(std::vector<EntityId> &out) {
    Signature sig;
    const ComponentTypeId t = ComponentTypeRegistry::GetTypeId<T>();
    const ComponentTypeId u = ComponentTypeRegistry::GetTypeId<U>();
    const ComponentTypeId v = ComponentTypeRegistry::GetTypeId<V>();
    const ComponentTypeId w = ComponentTypeRegistry::GetTypeId<W>();
    sig.add(t);
    sig.add(u);
    sig.add(v);
    sig.add(w);
    get_view_cached_into(sig, make_query_key({t, u, v, w}), out);
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
    query_cache_.clear();
    mutation_counter_ = 1;
  }

private:
  struct CachedQueryResult {
    uint64_t mutation = 0;
    std::vector<EntityId> entities;
  };

  void mark_mutated() { ++mutation_counter_; }

  static std::string make_query_key(std::initializer_list<ComponentTypeId> ids) {
    std::vector<ComponentTypeId> sorted(ids.begin(), ids.end());
    std::sort(sorted.begin(), sorted.end());
    std::string key;
    key.reserve(sorted.size() * 6);
    for (size_t i = 0; i < sorted.size(); ++i) {
      if (i > 0)
        key.push_back('|');
      key += std::to_string(sorted[i]);
    }
    return key;
  }

  std::vector<EntityId> get_view_cached(const Signature &sig, const std::string &key) {
    CachedQueryResult &entry = query_cache_[key];
    if (entry.mutation != mutation_counter_) {
      Query query(sig);
      query.build_into(entry.entities);
      entry.mutation = mutation_counter_;
    }
    return entry.entities;
  }

  void get_view_cached_into(const Signature &sig, const std::string &key,
                            std::vector<EntityId> &out) {
    CachedQueryResult &entry = query_cache_[key];
    if (entry.mutation != mutation_counter_) {
      Query query(sig);
      query.build_into(entry.entities);
      entry.mutation = mutation_counter_;
    }
    out = entry.entities;
  }

  std::unordered_map<ComponentTypeId, std::unique_ptr<IComponentStorage>>
      component_storage;
  std::unordered_map<std::string, CachedQueryResult> query_cache_;
  uint64_t mutation_counter_ = 1;
};

} // namespace criogenio::ecs
