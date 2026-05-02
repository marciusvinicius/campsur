#include "animated_component.h"
#include "animation_database.h"
#include "component_factory.h"
#include "components.h"
#include "core_systems.h"
#include "criogenio_io.h"
#include "game.h"
#include "map_event_system.h"
#include "player_anim.h"
#include "subterra_components.h"
#include "subterra_engine.h"
#include "subterra_interactable_prefabs.h"
#include "subterra_item_consumable.h"
#include "subterra_item_light.h"
#include "subterra_loadout.h"
#include "subterra_camera.h"
#include "subterra_input_config.h"
#include "subterra_player_vitals.h"
#include "subterra_server_config.h"
#include "subterra_session.h"
#include "subterra_status_effects.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace criogenio;
using namespace subterra;

namespace {

const char *kDefaultMapPath = "assets/levels/Cave.tmx";
const char *kDefaultPlayerJson = "assets/animations/player.json";
// Path order matches Odin reference: prefer cwd-relative data/... (SERVER_CONFIG_PATH,
// ENTITIES_JSON_PATH, INPUT_CONFIG_PATH), then subterra_guild/data/... when the binary is run
// from the campsur repo root.
const char *kItemPrefabJsonPaths[] = {
    "data/prefabs/entities_items.json",
    "subterra_guild/data/prefabs/entities_items.json",
};

const char *kInteractablePrefabJsonPaths[] = {
    "data/prefabs/entities_interactable.json",
    "subterra_guild/data/prefabs/entities_interactable.json",
};

const char *kWorldConfigPaths[] = {
    "data/server_configuration/world_config.json",
    "subterra_guild/data/server_configuration/world_config.json",
};

const char *kInputConfigPaths[] = {
    "data/server_configuration/input_config.json",
    "subterra_guild/data/server_configuration/input_config.json",
};

const char *kBuffersJsonPaths[] = {
    "data/effects/buffers.json",
    "subterra_guild/data/effects/buffers.json",
};

const char *kDebuffersJsonPaths[] = {
    "data/effects/debuffers.json",
    "subterra_guild/data/effects/debuffers.json",
};

void printUsage(const char *argv0) {
  std::fprintf(stderr,
               "Usage: %s [--world <path.json>] [--map <path.tmx>] [--player-json <path>] "
               "[--help]\n"
               "  --world     Load a full editor scene (entities + terrain + animations).\n"
               "              Skips default TMX + spawned player; uses Name \"player\" or "
               "PlayerTag.\n"
               "  Defaults: map %s , animations %s\n",
               argv0, kDefaultMapPath, kDefaultPlayerJson);
}

bool resolveLoadedPlayer(World &w, ecs::EntityId &outPlayer) {
  for (ecs::EntityId id : w.GetEntitiesWith<PlayerTag, Controller>()) {
    outPlayer = id;
    return true;
  }
  for (ecs::EntityId id : w.GetEntitiesWith<Name, Controller>()) {
    if (auto *n = w.GetComponent<Name>(id); n && n->name == "player") {
      outPlayer = id;
      return true;
    }
  }
  return false;
}

/** Prefer dimensions from the player's AnimatedSprite DB entry; else load player.json. */
void RegisterSubterraComponents() {
  ComponentFactory::Register("SubterraLoadout", [](World &w, int e) -> Component * {
    return &w.AddComponent<SubterraLoadout>(e);
  });
  ComponentFactory::Register("PlayerVitals", [](World &w, int e) -> Component * {
    return &w.AddComponent<PlayerVitals>(e);
  });
}

void syncPlayerAnimSession(World &w, SubterraSession &session,
                           const std::string &playerJsonPath) {
  session.playerAnimId = INVALID_ASSET_ID;
  session.playerW = 64;
  session.playerH = 64;
  if (session.player == ecs::NULL_ENTITY)
    return;
  if (auto *aspr = w.GetComponent<AnimatedSprite>(session.player);
      aspr && aspr->animationId != INVALID_ASSET_ID) {
    session.playerAnimId = aspr->animationId;
    if (const AnimationDef *def =
            AnimationDatabase::instance().getAnimation(aspr->animationId)) {
      if (!def->clips.empty() && !def->clips[0].frames.empty()) {
        const auto &r = def->clips[0].frames[0].rect;
        session.playerW = r.width;
        session.playerH = r.height;
        g_playerDrawW = r.width;
        g_playerDrawH = r.height;
        return;
      }
    }
  }
  PlayerAnimConfig cfg{};
  AssetId id = LoadPlayerAnimationDatabaseFromJson(playerJsonPath, &cfg);
  session.playerAnimId = id;
  session.playerW = cfg.frameW;
  session.playerH = cfg.frameH;
  g_playerDrawW = cfg.frameW;
  g_playerDrawH = cfg.frameH;
}

} // namespace

int main(int argc, char **argv) {
  std::string mapPath = kDefaultMapPath;
  std::string playerJsonPath = kDefaultPlayerJson;
  std::string worldJsonPath;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
      mapPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--player-json") == 0 && i + 1 < argc) {
      playerJsonPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--world") == 0 && i + 1 < argc) {
      worldJsonPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      printUsage(argv[0]);
      return 0;
    }
  }

  SubterraSession session;
  session.mapPath = mapPath;
  SubterraEngine engine(session, ScreenWidth, ScreenHeight, "Subterra Guild");
  session.engine = &engine;

  World &world = engine.GetWorld();
  session.world = &world;
  engine.RegisterCoreComponents();
  RegisterSubterraComponents();
  SubterraCameraApplyConfigDefaults(session.camera);
  SubterraInputApplyDefaults(session.input);
  bool item_lights_loaded = false;
  for (const char *p : kItemPrefabJsonPaths) {
    if (SubterraItemLight::TryLoadFromPath(p)) {
      std::printf("Item light prefabs: %s\n", p);
      item_lights_loaded = true;
      break;
    }
  }
  if (!item_lights_loaded)
    std::fprintf(stderr,
                 "Warning: could not load entities_items.json (item emitters). Tried "
                 "data/prefabs/ and subterra_guild/data/prefabs/.\n");

  bool interactable_ids_loaded = false;
  for (const char *p : kInteractablePrefabJsonPaths) {
    if (SubterraInteractablePrefabsTryLoadFromPath(p)) {
      std::printf("Interactable prefab ids: %s\n", p);
      interactable_ids_loaded = true;
      break;
    }
  }
  if (!interactable_ids_loaded)
    std::fprintf(stderr,
                 "Warning: could not load entities_interactable.json — mis-tagged "
                 "spawn_prefab objects may spawn as pickups.\n");

  if (SubterraTryLoadServerConfigurationFromPaths(
          kWorldConfigPaths, sizeof(kWorldConfigPaths) / sizeof(kWorldConfigPaths[0]), session)) {
    std::printf("Server configuration (world + camera): %s\n", session.worldConfigPath.c_str());
  } else {
    std::fprintf(stderr,
                 "Warning: could not load world_config.json — vitals/camera use built-in defaults.\n");
  }
  if (SubterraTryLoadInputConfigFromPaths(
          kInputConfigPaths, sizeof(kInputConfigPaths) / sizeof(kInputConfigPaths[0]), session)) {
    std::printf("Input configuration: %s\n", session.inputConfigPath.c_str());
  } else {
    std::fprintf(stderr,
                 "Warning: could not load input_config.json — using default keyboard bindings.\n");
  }
  {
    std::string statusErr;
    if (SubterraLoadStatusEffectsFromPaths(
            kBuffersJsonPaths, sizeof(kBuffersJsonPaths) / sizeof(kBuffersJsonPaths[0]),
            kDebuffersJsonPaths, sizeof(kDebuffersJsonPaths) / sizeof(kDebuffersJsonPaths[0]),
            session.statusRegistry, statusErr)) {
      std::printf("Status effects (buffers + debuffers) loaded.\n");
    } else {
      std::fprintf(stderr, "Warning: status effects: %s\n", statusErr.c_str());
    }
  }
  bool consumable_loaded = false;
  for (const char *p : kItemPrefabJsonPaths) {
    if (SubterraItemConsumableTryLoadFromPath(p)) {
      std::printf("Consumable restore data: %s\n", p);
      consumable_loaded = true;
      break;
    }
  }
  if (!consumable_loaded)
    std::fprintf(stderr,
                 "Warning: could not load consumable restore fields from entities_items.json\n");

  const bool fromWorldJson = !worldJsonPath.empty();

  if (fromWorldJson) {
    if (!LoadWorldFromFile(world, worldJsonPath)) {
      std::fprintf(stderr, "Failed to load world \"%s\".\n", worldJsonPath.c_str());
      return 1;
    }
    if (!resolveLoadedPlayer(world, session.player)) {
      std::fprintf(stderr,
                   "Loaded world has no player (need PlayerTag+Controller or Name \"player\" "
                   "+ Controller).\n");
      return 1;
    }
    syncPlayerAnimSession(world, session, playerJsonPath);
    if (session.playerAnimId == INVALID_ASSET_ID) {
      std::fprintf(stderr,
                   "No valid AnimatedSprite on player and failed to load \"%s\".\n",
                   playerJsonPath.c_str());
      return 1;
    }
    if (!world.GetComponent<Inventory>(session.player))
      world.AddComponent<Inventory>(session.player);
    if (!world.GetComponent<SubterraLoadout>(session.player))
      world.AddComponent<SubterraLoadout>(session.player);
    if (!world.GetComponent<PlayerVitals>(session.player)) {
      world.AddComponent<PlayerVitals>(session.player);
      SubterraInitPlayerVitals(*world.GetComponent<PlayerVitals>(session.player), session.worldRules);
    }
    session.mapPath = worldJsonPath;
    session.rebuildTriggers();
    SubterraApplyPlayerTileCollisionFootprint(world, session.player,
                                              static_cast<float>(session.playerW),
                                              static_cast<float>(session.playerH));
  } else {
    PlayerAnimConfig animCfg{};
    AssetId playerAnimId = LoadPlayerAnimationDatabaseFromJson(playerJsonPath, &animCfg);
    if (playerAnimId == INVALID_ASSET_ID) {
      std::fprintf(stderr, "Failed to load player animations from \"%s\".\n",
                   playerJsonPath.c_str());
      return 1;
    }
    session.playerAnimId = playerAnimId;
    session.playerW = animCfg.frameW;
    session.playerH = animCfg.frameH;
    g_playerDrawW = animCfg.frameW;
    g_playerDrawH = animCfg.frameH;

    std::string loadErr;
    if (!session.loadMap(mapPath, loadErr)) {
      std::fprintf(stderr, "Failed to load map \"%s\": %s\n", mapPath.c_str(), loadErr.c_str());
      return 1;
    }

    criogenio::Terrain2D *ter = world.GetTerrain();
    if (ter && ter->LogicalMapWidthTiles() > 0) {
      std::printf("Loaded TMX: %s (%d×%d tiles, %d×%d px cell)\n", mapPath.c_str(),
                  ter->LogicalMapWidthTiles(), ter->LogicalMapHeightTiles(), ter->GridStepX(),
                  ter->GridStepY());
    } else {
      std::printf("Loaded map: %s\n", mapPath.c_str());
    }
  }

  if (fromWorldJson) {
    criogenio::Terrain2D *ter = world.GetTerrain();
    if (ter && ter->LogicalMapWidthTiles() > 0) {
      std::printf("Loaded world: %s (%d×%d tiles, %d×%d px cell)\n", worldJsonPath.c_str(),
                  ter->LogicalMapWidthTiles(), ter->LogicalMapHeightTiles(), ter->GridStepX(),
                  ter->GridStepY());
    } else {
      std::printf("Loaded world: %s\n", worldJsonPath.c_str());
    }
  }

  world.AddSystem<MovementSystem>(world);
  world.AddSystem<MapBoundsSystem>(world);
  world.AddSystem<MapEventSystem>(session);
  world.AddSystem<AnimationSystem>(world);
  world.AddSystem<VitalsSystem>(session);
  world.AddSystem<CameraFollowSystem>(world, session);
  world.AddSystem<RenderSystem>(world);
  world.AddSystem<PickupSystem>(session);

  if (!fromWorldJson) {
    session.player = world.CreateEntity("player");
    world.AddComponent<Name>(session.player, "player");
    world.AddComponent<Transform>(session.player, 0.f, 0.f);
    world.AddComponent<Controller>(session.player, Vec2{PlayerMoveSpeed, PlayerMoveSpeed});
    world.AddComponent<AnimationState>(session.player);
    world.AddComponent<AnimatedSprite>(session.player, session.playerAnimId);
    world.AddComponent<PlayerTag>(session.player);
    world.AddComponent<Inventory>(session.player);
    world.AddComponent<SubterraLoadout>(session.player);
    world.AddComponent<PlayerVitals>(session.player);
    SubterraInitPlayerVitals(*world.GetComponent<PlayerVitals>(session.player), session.worldRules);

    session.placePlayerAtSpawn("");
    SubterraApplyPlayerTileCollisionFootprint(world, session.player,
                                              static_cast<float>(session.playerW),
                                              static_cast<float>(session.playerH));

    Camera2D cam = {};
    cam.offset = {0.f, 0.f};
    cam.target = {0.f, 0.f};
    cam.zoom = session.camera.cfg.zoom > 1e-4f ? session.camera.cfg.zoom : DefaultCameraZoom;
    world.AttachCamera2D(cam);
  } else if (world.GetMainCameraEntity() == ecs::NULL_ENTITY) {
    Camera2D cam = {};
    cam.offset = {0.f, 0.f};
    cam.target = {0.f, 0.f};
    cam.zoom = session.camera.cfg.zoom > 1e-4f ? session.camera.cfg.zoom : DefaultCameraZoom;
    world.AttachCamera2D(cam);
  }

  if (session.debugModeFromConfig)
    session.debugOverlay = true;

  engine.Run();
  return 0;
}
