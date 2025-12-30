#include "world2.h"
#include "animated_component.h"
#include "animation_database.h"
#include "asset_manager.h"
#include "resources.h"

namespace criogenio {

World2::World2() = default;

World2::~World2() { ecs::Registry::instance().clear(); }

ecs::EntityId World2::CreateEntity(const std::string &name) {
  ecs::EntityId id = ecs::Registry::instance().create_entity();
  if (!name.empty()) {
    entity_names[id] = name;
  }
  return id;
}

void World2::DeleteEntity(ecs::EntityId id) {
  ecs::Registry::instance().destroy_entity(id);
  entity_names.erase(id);
}

bool World2::HasEntity(ecs::EntityId id) const {
  return ecs::Registry::instance().has_entity(id);
}

void World2::OnUpdate(std::function<void(float)> fn) { userUpdate = fn; }

void World2::Update(float dt) {
  // Run user update callback
  if (userUpdate) {
    userUpdate(dt);
  }

  // Update systems
  for (auto &system : systems) {
    system->Update(dt);
  }
}

void World2::Render(Renderer &renderer) {
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

SerializedWorld World2::Serialize() const {
  SerializedWorld world;

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

void World2::Deserialize(const SerializedWorld &data) {
  ecs::Registry::instance().clear();

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

Terrain2D &World2::CreateTerrain2D(const std::string &name,
                                   const std::string &texture_path) {
  terrain = std::make_unique<Terrain2D>();

  // Initialize tileset similarly to legacy World::CreateTerrain2D
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

void World2::AttachCamera2D(Camera2D cam) { maincamera = cam; }

} // namespace criogenio
