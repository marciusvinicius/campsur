#include "world.h"
#include "animated_component.h"
#include "animation_database.h"
#include "asset_manager.h"
#include "log.h"
#include "resources.h"
#include <unordered_map>
#include <unordered_set>

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

void World::Render(Renderer& renderer) {
  renderer.BeginCamera2D(maincamera);
  renderer.DrawGrid(100, 32);
  renderer.DrawCircle(0, 0, 6, Colors::Red);

  if (terrain)
    terrain->Render(renderer);

  for (auto& system : systems) {
    system->Render(renderer);
  }

  renderer.EndCamera2D();
}

SerializedWorld World::Serialize() const {
  SerializedWorld world;

  if (terrain) {
    world.terrain = terrain->Serialize();
  }

  // Collect all animation IDs used by AnimatedSprite components
  std::unordered_set<AssetId> usedAnimationIds;
  for (ecs::EntityId entity_id : GetAllEntities()) {
    if (auto anim_sprite = GetComponent<AnimatedSprite>(entity_id)) {
      if (anim_sprite->animationId != INVALID_ASSET_ID) {
        usedAnimationIds.insert(anim_sprite->animationId);
      }
    }
  }

  // Serialize animation definitions
  for (AssetId animId : usedAnimationIds) {
    const auto *animDef = AnimationDatabase::instance().getAnimation(animId);
    if (!animDef)
      continue;

    SerializedAnimation serializedAnim;
    serializedAnim.id = animDef->id;
    serializedAnim.texturePath = animDef->texturePath;

    // Serialize clips
    for (const auto &clip : animDef->clips) {
      SerializedAnimationClip serializedClip;
      serializedClip.name = clip.name;
      serializedClip.frameSpeed = clip.frameSpeed;
      serializedClip.state = static_cast<int>(clip.state);
      serializedClip.direction = static_cast<int>(clip.direction);

      // Serialize frames
      for (const auto &frame : clip.frames) {
        SerializedAnimationFrame serializedFrame;
        serializedFrame.x = frame.rect.x;
        serializedFrame.y = frame.rect.y;
        serializedFrame.width = frame.rect.width;
        serializedFrame.height = frame.rect.height;
        serializedClip.frames.push_back(serializedFrame);
      }

      serializedAnim.clips.push_back(serializedClip);
    }

    world.animations.push_back(serializedAnim);
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

    if (auto nameComp = GetComponent<Name>(entity_id)) {
      serialized_entity.components.push_back(nameComp->Serialize());
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
  AnimationDatabase::instance().clear();

  // First, restore animation definitions BEFORE deserializing entities
  // This is critical because AnimatedSprite components reference animation IDs
  // Create a map to track animation ID remapping (old ID -> new ID)
  std::unordered_map<AssetId, AssetId> animIdMap;

  for (const auto &serializedAnim : data.animations) {
    // Validate texture path before creating animation
    if (serializedAnim.texturePath.empty()) {
      ENGINE_LOG(LOG_WARNING, "Animation ID %u has empty texture path, skipping",
                 serializedAnim.id);
      continue;
    }

    // Create animation in database
    AssetId createdId = AnimationDatabase::instance().createAnimation(
        serializedAnim.texturePath);

    // Load texture through asset manager and cache it
    auto texture = AssetManager::instance().load<TextureResource>(
        serializedAnim.texturePath);
    if (!texture) {
      ENGINE_LOG(LOG_ERROR,
                 "Failed to load texture for animation ID %u (new ID: %u): %s",
                 serializedAnim.id, createdId,
                 serializedAnim.texturePath.c_str());
      // Continue anyway - the animation will be created but texture won't be
      // available
    } else {
      ENGINE_LOG(LOG_INFO, "Loaded texture for animation ID %u (new ID: %u): %s",
                 serializedAnim.id, createdId,
                 serializedAnim.texturePath.c_str());
    }

    // Restore clips
    for (const auto &serializedClip : serializedAnim.clips) {
      AnimationClip clip;
      clip.name = serializedClip.name;
      clip.frameSpeed = serializedClip.frameSpeed;
      clip.state = static_cast<AnimState>(serializedClip.state);
      clip.direction = static_cast<Direction>(serializedClip.direction);

      // Restore frames
      for (const auto &serializedFrame : serializedClip.frames) {
        AnimationFrame frame;
        frame.rect.x = serializedFrame.x;
        frame.rect.y = serializedFrame.y;
        frame.rect.width = serializedFrame.width;
        frame.rect.height = serializedFrame.height;
        clip.frames.push_back(frame);
      }

      AnimationDatabase::instance().addClip(createdId, clip);
    }

    // Map old animation ID to new ID
    animIdMap[serializedAnim.id] = createdId;
  }

  if (data.animations.empty() && !data.entities.empty()) {
    // Check if any entities have AnimatedSprite components that reference
    // animations
    bool hasAnimatedSprites = false;
    for (const auto &ent : data.entities) {
      for (const auto &comp : ent.components) {
        if (comp.type == "AnimatedSprite") {
          hasAnimatedSprites = true;
          break;
        }
      }
      if (hasAnimatedSprites)
        break;
    }

    if (hasAnimatedSprites) {
      ENGINE_LOG(LOG_WARNING,
                 "World contains AnimatedSprite components but no "
                 "animation definitions were found. "
                 "Textures will not be loaded. Make sure to save "
                 "the world from the editor to include animations.");
    }
  }

  if (!data.terrain.tileset.tilesetPath.empty()) {
    terrain = std::make_unique<Terrain2D>();
    terrain->Deserialize(data.terrain);
  }

  for (const auto &serialized_entity : data.entities) {
    ecs::EntityId entity_id = CreateEntity();

    for (const auto &serialized_component : serialized_entity.components) {
      const auto &type_name = serialized_component.type;

      if (type_name == "Transform") {
        auto &trans = AddComponent<Transform>(entity_id);
        trans.Deserialize(serialized_component);
      } else if (type_name == "AnimationState") {
        auto &anim_state = AddComponent<AnimationState>(entity_id);
        anim_state.Deserialize(serialized_component);
      } else if (type_name == "Controller") {
        auto &ctrl = AddComponent<Controller>(entity_id);
        ctrl.Deserialize(serialized_component);
      } else if (type_name == "AIController") {
        auto &ai_ctrl = AddComponent<AIController>(entity_id);
        ai_ctrl.Deserialize(serialized_component);
      } else if (type_name == "AnimatedSprite") {
        auto &anim_sprite = AddComponent<AnimatedSprite>(entity_id);
        anim_sprite.Deserialize(serialized_component);
        // Map old animation ID to new ID if needed
        if (anim_sprite.animationId != INVALID_ASSET_ID) {
          auto it = animIdMap.find(anim_sprite.animationId);
          if (it != animIdMap.end()) {
            anim_sprite.animationId = it->second;
          } else {
            // Animation ID not found in map - this means the animation wasn't
            // serialized Log a warning and invalidate the animation ID
            ENGINE_LOG(LOG_WARNING,
                       "AnimatedSprite component references animation ID %u "
                       "which was not found in serialized data. "
                       "The animation may not have been saved. Entity ID: %d",
                       anim_sprite.animationId, entity_id);
            anim_sprite.animationId = INVALID_ASSET_ID;
          }
        }
        // Re-track reference for animation and ensure texture is loaded
        if (anim_sprite.animationId != INVALID_ASSET_ID) {
          AnimationDatabase::instance().addReference(anim_sprite.animationId);
          // Ensure texture is loaded for this animation
          const auto *animDef = AnimationDatabase::instance().getAnimation(
              anim_sprite.animationId);
          if (animDef && !animDef->texturePath.empty()) {
            auto texture = AssetManager::instance().load<TextureResource>(
                animDef->texturePath);
            if (!texture) {
              ENGINE_LOG(LOG_ERROR,
                         "Failed to load texture for animation ID %u: %s (Entity "
                         "ID: %d)",
                         anim_sprite.animationId, animDef->texturePath.c_str(),
                         entity_id);
            }
          }
        }
      } else if (type_name == "Name") {
        auto &nameComp = AddComponent<Name>(entity_id);
        nameComp.Deserialize(serialized_component);
      }
    }
  }
}

Terrain2D &World::CreateTerrain2D(const std::string &name,
                                  const std::string &texture_path) {
  terrain = std::make_unique<Terrain2D>();

  Tileset tileset{};
  tileset.tilesetPath = texture_path;
  tileset.atlas = AssetManager::instance().load<TextureResource>(texture_path);
  tileset.tileSize = 24;
  tileset.columns = 10;
  tileset.rows = 8;
  terrain->tileset = tileset;
  terrain->SetChunkSize(Terrain2D::kDefaultChunkSize);

  terrain->AddLayer();
  // Seed one chunk at (0,0) so there is something to paint
  for (int y = 0; y < terrain->GetChunkSize(); y++)
    for (int x = 0; x < terrain->GetChunkSize(); x++)
      terrain->SetTile(0, x, y, 8);

  return *terrain;
}

void World::AttachCamera2D(criogenio::Camera2D cam) { maincamera = cam; }

Terrain2D *World::GetTerrain() { return terrain.get(); }

} // namespace criogenio
