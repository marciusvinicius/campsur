#include "world.h"
#include "animated_component.h"
#include "animation_database.h"
#include "asset_manager.h"
#include "resources.h"

namespace criogenio {

World::World() = default;

World::~World() {
  systems.clear(); // Clear all systems explicitly
  ecs::Registry::instance().clear();
}

ecs::EntityId World::CreateEntity(const std::string &name) {
  ecs::EntityId id = ecs::Registry::instance().create_entity();
  if (!name.empty()) {
    entity_names[id] = name;
  }
  return id;
}

void World::DeleteEntity(ecs::EntityId id) {
  ecs::Registry::instance().destroy_entity(id);
  entity_names.erase(id);
}

bool World::HasEntity(ecs::EntityId id) const {
  return ecs::Registry::instance().has_entity(id);
}

void World::OnUpdate(std::function<void(float)> fn) { userUpdate = fn; }

void World::Update(float dt) {
  // Run user update callback
  if (userUpdate) {
    userUpdate(dt);
  }

  // Update systems
  for (auto &system : systems) {
    system->Update(dt);
  }
}

void World::Render(Renderer &renderer) {
  BeginMode2D(maincamera);
  DrawGrid(100, 32);
  DrawCircle(0, 0, 6, RED);

  if (terrain)
    terrain->Render(renderer);

  // Render systems
  for (auto &system : systems) {
    system->Render(renderer);
  }

  EndMode2D();
}

SerializedWorld World::Serialize() const {
  SerializedWorld world;

  // Serialize terrain if it exists
  if (terrain) {
    // Serialize tileset
    world.terrain.tileset.tilesetPath = terrain->tileset.tilesetPath;
    world.terrain.tileset.tileSize = terrain->tileset.tileSize;
    world.terrain.tileset.columns = terrain->tileset.columns;
    world.terrain.tileset.rows = terrain->tileset.rows;

    // Serialize layers
    for (const auto &layer : terrain->layers) {
      SerializedTileLayer serialized_layer;
      serialized_layer.width = layer.width;
      serialized_layer.height = layer.height;
      serialized_layer.tiles = layer.tiles;
      world.terrain.layers.push_back(serialized_layer);
    }
  }

  for (ecs::EntityId entity_id : GetAllEntities()) {
    SerializedEntity serialized_entity;
    serialized_entity.id = entity_id;

    // Serialize Transform if exists
    if (auto trans = GetComponent<Transform>(entity_id)) {
      serialized_entity.components.push_back(trans->Serialize());
    }

    // Serialize AnimationState if exists
    if (auto anim_state = GetComponent<AnimationState>(entity_id)) {
      serialized_entity.components.push_back(anim_state->Serialize());
    }

    // Serialize Controller if exists
    if (auto ctrl = GetComponent<Controller>(entity_id)) {
      serialized_entity.components.push_back(ctrl->Serialize());
    }

    // Serialize AIController if exists
    if (auto ai_ctrl = GetComponent<AIController>(entity_id)) {
      serialized_entity.components.push_back(ai_ctrl->Serialize());
    }

    // Serialize AnimatedSprite if exists
    if (auto anim_sprite = GetComponent<AnimatedSprite>(entity_id)) {
      serialized_entity.components.push_back(anim_sprite->Serialize());
    }

    if (!serialized_entity.components.empty()) {
      world.entities.push_back(serialized_entity);
    }
  }

  return world;
}

void World::Deserialize(const SerializedWorld &data) {
  ecs::Registry::instance().clear();
  terrain = nullptr;

  // Restore terrain if it exists
  if (!data.terrain.tileset.tilesetPath.empty()) {
    terrain = std::make_unique<Terrain2D>();

    // Restore tileset
    Tileset tileset{};
    tileset.tilesetPath = data.terrain.tileset.tilesetPath;
    tileset.atlas =
        AssetManager::instance().load<TextureResource>(tileset.tilesetPath);
    tileset.tileSize = data.terrain.tileset.tileSize;
    tileset.columns = data.terrain.tileset.columns;
    tileset.rows = data.terrain.tileset.rows;
    terrain->tileset = tileset;

    // Restore layers
    for (const auto &serialized_layer : data.terrain.layers) {
      terrain->AddLayer(serialized_layer.width, serialized_layer.height, 8);
      // Restore tiles for this layer (assuming we add to the last layer)
      if (!terrain->layers.empty()) {
        terrain->layers.back().tiles = serialized_layer.tiles;
      }
    }
  }

  for (const auto &serialized_entity : data.entities) {
    ecs::EntityId entity_id = CreateEntity();

    for (const auto &serialized_component : serialized_entity.components) {
      const auto &type_name = serialized_component.type;

      if (type_name == "Transform") {
        auto &trans = AddComponent<Transform>(entity_id);
        trans.Deserialize(serialized_component);
      }

      else if (type_name == "AnimationState") {
        auto &anim_state = AddComponent<AnimationState>(entity_id);
        anim_state.Deserialize(serialized_component);
      }

      else if (type_name == "Controller") {
        auto &ctrl = AddComponent<Controller>(entity_id);
        ctrl.Deserialize(serialized_component);
      }

      else if (type_name == "AIController") {
        auto &ai_ctrl = AddComponent<AIController>(entity_id);
        ai_ctrl.Deserialize(serialized_component);
      }

      else if (type_name == "AnimatedSprite") {
        auto &anim_sprite = AddComponent<AnimatedSprite>(entity_id);
        anim_sprite.Deserialize(serialized_component);

        // Re-track reference for animation
        if (anim_sprite.animationId != 0) {
          AnimationDatabase::instance().addReference(anim_sprite.animationId);
        }
      }
    }
  }
}

Terrain2D &World::CreateTerrain2D(const std::string &name,
                                  const std::string &texture_path) {
  terrain = std::make_unique<Terrain2D>();

  // Initialize tileset similarly to World::CreateTerrain2D
  Tileset tileset{};
  tileset.tilesetPath = texture_path;
  tileset.atlas = AssetManager::instance().load<TextureResource>(texture_path);
  tileset.tileSize = 24;
  tileset.columns = 10;
  tileset.rows = 8;
  terrain->tileset = tileset;

  // Create a default layer so something is visible
  terrain->AddLayer(10, 10, 8);

  return *terrain;
}

void World::AttachCamera2D(Camera2D cam) { maincamera = cam; }

Terrain2D *World::GetTerrain() { return terrain.get(); }

} // namespace criogenio
