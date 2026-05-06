#include "world.h"
#include "animated_component.h"
#include "animation_database.h"
#include "asset_manager.h"
#include "gameplay_tags.h"
#include "inventory.h"
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
  renderer.BeginCamera2D(*GetActiveCamera());
  // Only draw the default grid when there is no terrain; otherwise the editor
  // draws a single terrain-aligned grid to avoid two overlapping grids.
  if (!terrain) {
    renderer.DrawGrid(100, 32);
    renderer.DrawCircle(0, 0, 6, Colors::Red);
  }

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
    if (auto trans3D = GetComponent<Transform3D>(entity_id)) {
      serialized_entity.components.push_back(trans3D->Serialize());
    }

    // Serialize AnimationState if exists
    if (auto anim_state = GetComponent<AnimationState>(entity_id)) {
      serialized_entity.components.push_back(anim_state->Serialize());
    }

    // Serialize Controller if exists
    if (auto ctrl = GetComponent<Controller>(entity_id)) {
      serialized_entity.components.push_back(ctrl->Serialize());
    }
    if (auto ctrl3D = GetComponent<PlayerController3D>(entity_id)) {
      serialized_entity.components.push_back(ctrl3D->Serialize());
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
    if (auto rl = GetComponent<SpriteRenderLayer>(entity_id)) {
      serialized_entity.components.push_back(rl->Serialize());
    }

    if (auto bc = GetComponent<BoxCollider>(entity_id)) {
      serialized_entity.components.push_back(bc->Serialize());
    }
    if (auto rb = GetComponent<RigidBody>(entity_id)) {
      serialized_entity.components.push_back(rb->Serialize());
    }
    if (auto gd = GetComponent<Grounded>(entity_id)) {
      serialized_entity.components.push_back(gd->Serialize());
    }
    if (auto spr = GetComponent<Sprite>(entity_id)) {
      serialized_entity.components.push_back(spr->Serialize());
    }
    if (auto nr = GetComponent<NetReplicated>(entity_id)) {
      serialized_entity.components.push_back(nr->Serialize());
    }
    if (auto netId = GetComponent<ReplicatedNetId>(entity_id)) {
      serialized_entity.components.push_back(netId->Serialize());
    }
    if (auto pt = GetComponent<PlayerTag>(entity_id)) {
      serialized_entity.components.push_back(pt->Serialize());
    }
    if (auto mt = GetComponent<MobTag>(entity_id)) {
      serialized_entity.components.push_back(mt->Serialize());
    }
    if (auto wp = GetComponent<WorldPickup>(entity_id)) {
      serialized_entity.components.push_back(wp->Serialize());
    }
    if (auto inv = GetComponent<Inventory>(entity_id)) {
      serialized_entity.components.push_back(inv->Serialize());
    }

    if (auto cam = GetComponent<Camera>(entity_id)) {
      serialized_entity.components.push_back(cam->Serialize());
    }
    if (auto cam3D = GetComponent<Camera3D>(entity_id)) {
      serialized_entity.components.push_back(cam3D->Serialize());
    }
    if (auto model3D = GetComponent<Model3D>(entity_id)) {
      serialized_entity.components.push_back(model3D->Serialize());
    }
    if (auto boxCollider3D = GetComponent<BoxCollider3D>(entity_id)) {
      serialized_entity.components.push_back(boxCollider3D->Serialize());
    }
    if (auto box3D = GetComponent<Box3D>(entity_id)) {
      serialized_entity.components.push_back(box3D->Serialize());
    }

    if (!serialized_entity.components.empty()) {
      world.entities.push_back(serialized_entity);
    }
  }

  return world;
}

void World::Deserialize(const SerializedWorld &data,
                        const std::string &asset_root_dir) {
  ecs::Registry::instance().clear();
  mainCameraEntity = ecs::NULL_ENTITY;
  mainCamera3DEntity = ecs::NULL_ENTITY;
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

    const std::string texPath =
        ResolvePathRelativeToWorldFile(asset_root_dir, serializedAnim.texturePath);

    // Create animation in database
    AssetId createdId = AnimationDatabase::instance().createAnimation(texPath);

    // Load texture through asset manager and cache it
    auto texture = AssetManager::instance().load<TextureResource>(texPath);
    if (!texture) {
      ENGINE_LOG(LOG_ERROR,
                 "Failed to load texture for animation ID %u (new ID: %u): %s",
                 serializedAnim.id, createdId, texPath.c_str());
      // Continue anyway - the animation will be created but texture won't be
      // available
    } else {
      ENGINE_LOG(LOG_INFO, "Loaded texture for animation ID %u (new ID: %u): %s",
                 serializedAnim.id, createdId, texPath.c_str());
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
    terrain->Deserialize(data.terrain, asset_root_dir);
  }

  for (const auto &serialized_entity : data.entities) {
    ecs::EntityId entity_id = CreateEntity();

    for (const auto &serialized_component : serialized_entity.components) {
      const auto &type_name = serialized_component.type;

      if (type_name == "Transform") {
        auto &trans = AddComponent<Transform>(entity_id);
        trans.Deserialize(serialized_component);
      } else if (type_name == "Transform3D") {
        auto &trans3D = AddComponent<Transform3D>(entity_id);
        trans3D.Deserialize(serialized_component);
      } else if (type_name == "AnimationState") {
        auto &anim_state = AddComponent<AnimationState>(entity_id);
        anim_state.Deserialize(serialized_component);
      } else if (type_name == "Controller") {
        auto &ctrl = AddComponent<Controller>(entity_id);
        ctrl.Deserialize(serialized_component);
      } else if (type_name == "PlayerController3D") {
        auto &ctrl3D = AddComponent<PlayerController3D>(entity_id);
        ctrl3D.Deserialize(serialized_component);
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
      } else if (type_name == "SpriteRenderLayer") {
        auto &rl = AddComponent<SpriteRenderLayer>(entity_id);
        rl.Deserialize(serialized_component);
      } else if (type_name == "RigidBody") {
        auto &rb = AddComponent<RigidBody>(entity_id);
        rb.Deserialize(serialized_component);
      } else if (type_name == "Grounded") {
        auto &gd = AddComponent<Grounded>(entity_id);
        gd.Deserialize(serialized_component);
      } else if (type_name == "Sprite") {
        auto &spr = AddComponent<Sprite>(entity_id);
        spr.Deserialize(serialized_component);
      } else if (type_name == "NetReplicated") {
        AddComponent<NetReplicated>(entity_id).Deserialize(serialized_component);
      } else if (type_name == "ReplicatedNetId") {
        AddComponent<ReplicatedNetId>(entity_id).Deserialize(serialized_component);
      } else if (type_name == "PlayerTag") {
        AddComponent<PlayerTag>(entity_id).Deserialize(serialized_component);
      } else if (type_name == "MobTag") {
        AddComponent<MobTag>(entity_id).Deserialize(serialized_component);
      } else if (type_name == "WorldPickup") {
        auto &wp = AddComponent<WorldPickup>(entity_id);
        wp.Deserialize(serialized_component);
      } else if (type_name == "Inventory") {
        auto &inv = AddComponent<Inventory>(entity_id);
        inv.Deserialize(serialized_component);
      } else if (type_name == "BoxCollider") {
        auto &col = AddComponent<BoxCollider>(entity_id);
        col.Deserialize(serialized_component);
      } else if (type_name == "Camera" || type_name == "Camera2D") {
        auto &cam = AddComponent<Camera>(entity_id);
        cam.Deserialize(serialized_component);
        if (mainCameraEntity == ecs::NULL_ENTITY)
          mainCameraEntity = entity_id;
      } else if (type_name == "Camera3D") {
        auto &cam3D = AddComponent<Camera3D>(entity_id);
        cam3D.Deserialize(serialized_component);
        if (mainCamera3DEntity == ecs::NULL_ENTITY)
          mainCamera3DEntity = entity_id;
      } else if (type_name == "Model3D") {
        auto &model3D = AddComponent<Model3D>(entity_id);
        model3D.Deserialize(serialized_component);
      } else if (type_name == "BoxCollider3D") {
        auto &boxCollider3D = AddComponent<BoxCollider3D>(entity_id);
        boxCollider3D.Deserialize(serialized_component);
      } else if (type_name == "Box3D") {
        auto &box3D = AddComponent<Box3D>(entity_id);
        box3D.Deserialize(serialized_component);
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

Camera2D* World::GetActiveCamera() {
  if (mainCameraEntity != ecs::NULL_ENTITY) {
    if (Camera* c = GetComponent<Camera>(mainCameraEntity))
      return &c->data;
  }
  return &maincamera;
}

const Camera2D* World::GetActiveCamera() const {
  if (mainCameraEntity != ecs::NULL_ENTITY) {
    if (const Camera* c = GetComponent<Camera>(mainCameraEntity))
      return &c->data;
  }
  return &maincamera;
}

box3d::FPCamera* World::GetActiveCamera3D() {
  if (mainCamera3DEntity != ecs::NULL_ENTITY) {
    if (Camera3D* c = GetComponent<Camera3D>(mainCamera3DEntity)) {
      maincamera3D = c->ToFPCamera();
      return &maincamera3D;
    }
  }
  return &maincamera3D;
}

const box3d::FPCamera* World::GetActiveCamera3D() const {
  if (mainCamera3DEntity != ecs::NULL_ENTITY) {
    if (const Camera3D* c = GetComponent<Camera3D>(mainCamera3DEntity)) {
      const_cast<World*>(this)->maincamera3D = c->ToFPCamera();
      return &maincamera3D;
    }
  }
  return &maincamera3D;
}

void World::AttachCamera2D(criogenio::Camera2D cam) {
  maincamera = cam;
  ecs::EntityId e = CreateEntity("MainCamera");
  AddComponent<Camera>(e, Camera(cam));
  AddComponent<Name>(e, "MainCamera");
  mainCameraEntity = e;
}

void World::AttachCamera3D(const box3d::FPCamera& cam) {
  maincamera3D = cam;
  ecs::EntityId e = CreateEntity("MainCamera3D");
  AddComponent<Camera3D>(e, Camera3D(cam));
  AddComponent<Name>(e, "MainCamera3D");
  mainCamera3DEntity = e;
}

Terrain2D *World::GetTerrain() { return terrain.get(); }

void World::SetTerrain(std::unique_ptr<Terrain2D> t) { terrain = std::move(t); }

void World::DeleteTerrain() { terrain = nullptr; }

} // namespace criogenio
