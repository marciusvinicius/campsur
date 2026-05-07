#include "editor_game_mode.h"
#include "editor.h"
#include "project_context.h"

// Engine / IO
#include "criogenio_io.h"
#include "json_serialization.h"
#include "json.hpp"
#include "world.h"
#include "components.h"
#include "component_factory.h"
#include "core_systems.h"
#include "animated_component.h"

// Subterra gameplay
#include "game.h"
#include "map_event_system.h"
#include "player_anim.h"
#include "subterra_camera.h"
#include "subterra_components.h"
#include "subterra_day_night.h"
#include "subterra_gameplay_actions.h"
#include "subterra_imgui.h"
#include "subterra_input_config.h"
#include "subterra_interactable_prefabs.h"
#include "subterra_item_consumable.h"
#include "subterra_item_light.h"
#include "subterra_item_light_runtime.h"
#include "subterra_loadout.h"
#include "subterra_mob_prefabs.h"
#include "subterra_player_vitals.h"
#include "subterra_server_config.h"
#include "subterra_session.h"
#include "spawn_service.h"
#include "subterra_status_effects.h"

// ImGui
#include "imgui.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

using namespace criogenio;

namespace {

static constexpr float kPlayModeZoom = 1.15f;
static constexpr float kPlayerMoveSpeed = 220.f;

static bool FileReadable(const char *p) {
    std::ifstream f(p);
    return f.good();
}

static const char *kItemPrefabJsonPaths[] = {
    "data/prefabs/entities_items.campsurmeta",
    "subterra_guild/data/prefabs/entities_items.campsurmeta",
};
static const char *kInteractablePrefabJsonPaths[] = {
    "data/prefabs/entities_interactable.campsurmeta",
    "subterra_guild/data/prefabs/entities_interactable.campsurmeta",
};
static const char *kMobPrefabJsonPaths[] = {
    "data/prefabs/entities_mobs.campsurmeta",
    "subterra_guild/data/prefabs/entities_mobs.campsurmeta",
};
static const char *kWorldConfigPaths[] = {
    "data/server_configuration/world_config.campsurconfig",
    "subterra_guild/data/server_configuration/world_config.campsurconfig",
};
static const char *kInputConfigPaths[] = {
    "data/server_configuration/input_config.campsurconfig",
    "subterra_guild/data/server_configuration/input_config.campsurconfig",
};
static const char *kBuffersJsonPaths[] = {
    "data/effects/buffers.campsurconfig",
    "subterra_guild/data/effects/buffers.campsurconfig",
};
static const char *kDebuffersJsonPaths[] = {
    "data/effects/debuffers.campsurconfig",
    "subterra_guild/data/effects/debuffers.campsurconfig",
};
static const char *kPlayerJsonPaths[] = {
    "assets/animations/player.campsuranims",
    "data/animations/player.campsuranims",
    "subterra_guild/data/animations/player.campsuranims",
};

/** Try each path in turn; return the first that loads successfully, or INVALID_ASSET_ID. */
static AssetId LoadPlayerAnimTryPaths(subterra::PlayerAnimConfig *cfgOut) {
    for (const char *p : kPlayerJsonPaths) {
        if (!FileReadable(p))
            continue;
        AssetId id = subterra::LoadPlayerAnimationDatabaseFromJson(p, cfgOut);
        if (id != INVALID_ASSET_ID) {
            std::printf("[PlayMode] Player animations: %s\n", p);
            return id;
        }
    }
    return INVALID_ASSET_ID;
}

/** Find an existing player entity (PlayerTag+Controller or Name=="player"+Controller). */
static ecs::EntityId FindPlayerEntity(World &w) {
    for (ecs::EntityId id : w.GetEntitiesWith<subterra::PlayerTag, Controller>())
        return id;
    for (ecs::EntityId id : w.GetEntitiesWith<Name, Controller>()) {
        if (auto *n = w.GetComponent<Name>(id); n && n->name == "player")
            return id;
    }
    return ecs::NULL_ENTITY;
}

} // namespace

namespace subterra {

void EditorGameMode::Start(EditorApp &app, const ProjectContext *proj) {
    World &world = app.GetWorld();

    // ---- (1) Save the editor camera for later restoration -------------------
    editorCamera = *world.GetActiveCamera();

    // ---- (2) Snapshot the entire world to JSON string -----------------------
    try {
        nlohmann::json j = ToJson(world.Serialize("."));
        worldSnapshot = j.dump();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[PlayMode] WARNING: world snapshot failed: %s\n", e.what());
        worldSnapshot.clear();
    }

    // ---- (3) Bind session pointers ------------------------------------------
    session.world = &world;
    session.engine = &app;

    // ---- (4) Load world_config (camera, vitals rules, etc.) ----------------
    // When a project is open, resolve paths directly from project.campsur game_config.
    // Otherwise fall back to the hardcoded search arrays.
    if (proj && !proj->gameConfig.is_null()) {
        std::string worldCfg = proj->Resolve(proj->gameConfig.value("world_config", ""));
        if (!worldCfg.empty()) {
            const char *paths[] = { worldCfg.c_str() };
            if (SubterraTryLoadServerConfigurationFromPaths(paths, 1, session))
                std::printf("[PlayMode] Server config (project): %s\n", worldCfg.c_str());
            else
                std::fprintf(stderr, "[PlayMode] Warning: world_config not found at %s\n", worldCfg.c_str());
        }
        std::string inputCfg = proj->Resolve(proj->gameConfig.value("input_config", ""));
        if (!inputCfg.empty()) {
            const char *paths[] = { inputCfg.c_str() };
            SubterraTryLoadInputConfigFromPaths(paths, 1, session);
        }
    } else {
        if (SubterraTryLoadServerConfigurationFromPaths(
                kWorldConfigPaths, sizeof(kWorldConfigPaths) / sizeof(kWorldConfigPaths[0]), session)) {
            std::printf("[PlayMode] Server config: %s\n", session.worldConfigPath.c_str());
        } else {
            std::fprintf(stderr, "[PlayMode] Warning: world_config.campsurconfig not found — using defaults.\n");
        }
    }
    SubterraCameraApplyConfigDefaults(session.camera);
    SubterraInputApplyDefaults(session.input);
    RegisterDefaultSubterraGameplayActions(session.gameplay);
    RegisterSubterraGameplayMapEventHandler(session);

    // ---- (5) Register Subterra component types ------------------------------
    // RegisterSubterraComponents is defined in main.cpp (static helpers).
    // We replicate inline here since it's in an anonymous namespace there.
    ComponentFactory::Register("SubterraLoadout", [](World &w, int e) -> Component * {
        return &w.AddComponent<SubterraLoadout>(e);
    });
    ComponentFactory::Register("PlayerVitals", [](World &w, int e) -> Component * {
        return &w.AddComponent<PlayerVitals>(e);
    });
    ComponentFactory::Register("ItemLightEmitterState", [](World &w, int e) -> Component * {
        return &w.AddComponent<ItemLightEmitterState>(e);
    });

    // ---- (6) Load prefab databases (item lights, interactables, mobs) ------
    // Helper: try a project-resolved path first, then fall back to search arrays.
    auto tryLoadOne = [&](const char *projRelKey, const char *const *fallback, size_t fallbackN,
                          std::function<bool(const char *)> loader,
                          const char *label) {
        if (proj && !proj->gameConfig.is_null()) {
            std::string resolved = proj->prefabsDir;
            if (!resolved.empty()) {
                // Build candidate from prefabs dir + filename suffix
                std::string candidate = (std::filesystem::path(resolved) /
                    (std::string(projRelKey) + ".campsurmeta")).string();
                if (FileReadable(candidate.c_str()) && loader(candidate.c_str())) {
                    std::printf("[PlayMode] %s (project): %s\n", label, candidate.c_str());
                    return;
                }
            }
        }
        for (size_t i = 0; i < fallbackN; ++i) {
            if (loader(fallback[i])) {
                std::printf("[PlayMode] %s: %s\n", label, fallback[i]);
                return;
            }
        }
        std::fprintf(stderr, "[PlayMode] Warning: %s not found.\n", label);
    };

    tryLoadOne("entities_items", kItemPrefabJsonPaths,
               sizeof(kItemPrefabJsonPaths)/sizeof(kItemPrefabJsonPaths[0]),
               [](const char *p){ return SubterraItemLight::TryLoadFromPath(p); },
               "Item light prefabs");
    tryLoadOne("entities_items", kItemPrefabJsonPaths,
               sizeof(kItemPrefabJsonPaths)/sizeof(kItemPrefabJsonPaths[0]),
               [](const char *p){ return SubterraItemConsumableTryLoadFromPath(p); },
               "Consumable prefabs");
    tryLoadOne("entities_interactable", kInteractablePrefabJsonPaths,
               sizeof(kInteractablePrefabJsonPaths)/sizeof(kInteractablePrefabJsonPaths[0]),
               [](const char *p){ return SubterraInteractablePrefabsTryLoadFromPath(p); },
               "Interactable prefabs");
    tryLoadOne("entities_mobs", kMobPrefabJsonPaths,
               sizeof(kMobPrefabJsonPaths)/sizeof(kMobPrefabJsonPaths[0]),
               [](const char *p){ return SubterraMobPrefabsTryLoadFromPath(p); },
               "Mob prefabs");

    // ---- (7) Load input and status effects (best-effort) -------------------
    // Input config: project path takes priority over search arrays.
    if (proj && session.inputConfigPath.empty()) {
        // Already loaded in step 4 if project provided input_config
    }
    if (session.inputConfigPath.empty()) {
        if (SubterraTryLoadInputConfigFromPaths(
                kInputConfigPaths, sizeof(kInputConfigPaths) / sizeof(kInputConfigPaths[0]), session)) {
            std::printf("[PlayMode] Input config: %s\n", session.inputConfigPath.c_str());
        } else {
            std::fprintf(stderr, "[PlayMode] Warning: input_config.campsurconfig not found — using defaults.\n");
        }
    }
    {
        // Status effects: try project effects dir, then fallback.
        std::string bufs, debufs;
        if (proj && !proj->effectsDir.empty()) {
            bufs   = (std::filesystem::path(proj->effectsDir) / "buffers.campsurconfig").string();
            debufs = (std::filesystem::path(proj->effectsDir) / "debuffers.campsurconfig").string();
        }
        const char *bufsArr[]   = { bufs.empty()   ? kBuffersJsonPaths[0]   : bufs.c_str(),
                                    kBuffersJsonPaths[0], kBuffersJsonPaths[1] };
        const char *debufsArr[] = { debufs.empty() ? kDebuffersJsonPaths[0] : debufs.c_str(),
                                    kDebuffersJsonPaths[0], kDebuffersJsonPaths[1] };
        const size_t nb = bufs.empty() ? 2 : 3;
        const size_t nd = debufs.empty() ? 2 : 3;
        std::string statusErr;
        if (!SubterraLoadStatusEffectsFromPaths(bufsArr, nb, debufsArr, nd,
                                                session.statusRegistry, statusErr)) {
            std::fprintf(stderr, "[PlayMode] Warning: status effects: %s\n", statusErr.c_str());
        }
    }

    // ---- (8) Load player animations ----------------------------------------
    PlayerAnimConfig animCfg{};
    AssetId playerAnimId = INVALID_ASSET_ID;
    if (proj && !proj->gameConfig.is_null()) {
        std::string playerAnims = proj->Resolve(proj->gameConfig.value("player_anims", ""));
        if (!playerAnims.empty() && FileReadable(playerAnims.c_str())) {
            playerAnimId = LoadPlayerAnimationDatabaseFromJson(playerAnims, &animCfg);
            if (playerAnimId != INVALID_ASSET_ID)
                std::printf("[PlayMode] Player animations (project): %s\n", playerAnims.c_str());
        }
    }
    if (playerAnimId == INVALID_ASSET_ID)
        playerAnimId = LoadPlayerAnimTryPaths(&animCfg);

    if (playerAnimId == INVALID_ASSET_ID) {
        std::fprintf(stderr, "[PlayMode] Warning: player animation not found — player may be invisible.\n");
    } else {
        session.playerAnimId = playerAnimId;
        session.playerW = animCfg.frameW;
        session.playerH = animCfg.frameH;
        subterra::g_playerDrawW = animCfg.frameW;
        subterra::g_playerDrawH = animCfg.frameH;
    }

    // ---- (9) Swap systems: editor → Subterra --------------------------------
    app.GetWorld().ClearSystems();
    world.AddSystem<MovementSystem>(world);
    world.AddSystem<MobBrainSystem>(session);
    world.AddSystem<AIMovementSystem>(world);
    world.AddSystem<MapBoundsSystem>(world);
    world.AddSystem<MapEventSystem>(session);
    world.AddSystem<ItemLightSyncSystem>(session);
    world.AddSystem<ItemEventDispatchSystem>(session);
    world.AddSystem<AnimationSystem>(world);
    world.AddSystem<VitalsSystem>(session);
    world.AddSystem<CameraFollowSystem>(world, session);
    world.AddSystem<RenderSystem>(world);
    world.AddSystem<PickupSystem>(session);

    // ---- (10) Locate or create the player entity ----------------------------
    session.player = FindPlayerEntity(world);
    if (session.player == ecs::NULL_ENTITY) {
        session.player = world.CreateEntity("player");
        world.AddComponent<Name>(session.player, "player");
        world.AddComponent<Transform>(session.player, 2.f, 2.f);
        world.AddComponent<Controller>(session.player, Vec2{kPlayerMoveSpeed, kPlayerMoveSpeed});
        world.AddComponent<AnimationState>(session.player);
        if (session.playerAnimId != INVALID_ASSET_ID)
            world.AddComponent<AnimatedSprite>(session.player, session.playerAnimId);
        world.AddComponent<PlayerTag>(session.player);
        world.AddComponent<Inventory>(session.player);
        world.AddComponent<SubterraLoadout>(session.player);
        world.AddComponent<PlayerVitals>(session.player);
        SubterraInitPlayerVitals(*world.GetComponent<PlayerVitals>(session.player), session.worldRules);
    } else {
        // Ensure required components are present on a designer-placed player.
        if (!world.HasComponent<PlayerTag>(session.player))
            world.AddComponent<PlayerTag>(session.player);
        if (!world.HasComponent<AnimationState>(session.player))
            world.AddComponent<AnimationState>(session.player);
        if (!world.HasComponent<Inventory>(session.player))
            world.AddComponent<Inventory>(session.player);
        if (!world.HasComponent<SubterraLoadout>(session.player))
            world.AddComponent<SubterraLoadout>(session.player);
        if (!world.HasComponent<PlayerVitals>(session.player)) {
            world.AddComponent<PlayerVitals>(session.player);
            SubterraInitPlayerVitals(*world.GetComponent<PlayerVitals>(session.player), session.worldRules);
        }
        if (session.playerAnimId != INVALID_ASSET_ID &&
            !world.HasComponent<AnimatedSprite>(session.player)) {
            world.AddComponent<AnimatedSprite>(session.player, session.playerAnimId);
        }
    }

    SubterraApplyPlayerTileCollisionFootprint(world, session.player,
                                              static_cast<float>(session.playerW),
                                              static_cast<float>(session.playerH));

    // ---- (11) Set up camera -------------------------------------------------
    Camera2D cam = {};
    cam.offset = {0.f, 0.f};
    cam.target = {0.f, 0.f};
    float cfgZoom = session.camera.cfg.zoom;
    cam.zoom = cfgZoom > 1e-4f ? cfgZoom : kPlayModeZoom;
    world.AttachCamera2D(cam);

    // ---- (12) Spawn mobs, triggers, prefabs from terrain -------------------
    session.mapPath = "";
    session.applyPostTerrainLoad("");
    RegisterAuthoringMobSessionState(session);
    session.placePlayerAtSpawn("");

    std::printf("[PlayMode] ▶ Started.\n");
}

void EditorGameMode::Stop(EditorApp &app, bool revert) {
    World &world = app.GetWorld();

    // Remove mobs and world pickups.
    session.destroyTransientEntities();

    // Remove the player entity.
    if (session.player != ecs::NULL_ENTITY && world.HasEntity(session.player)) {
        world.DeleteEntity(session.player);
        session.player = ecs::NULL_ENTITY;
    }

    // Swap systems back: Subterra → editor set.
    world.ClearSystems();
    app.RegisterEditorSystems();

    // Restore pre-play world state from snapshot.
    if (revert && !worldSnapshot.empty()) {
        try {
            nlohmann::json j = nlohmann::json::parse(worldSnapshot);
            SerializedWorld sw = FromJson(j);
            world.Deserialize(sw, ".");
            std::printf("[PlayMode] World reverted to pre-play snapshot.\n");
        } catch (const std::exception &e) {
            std::fprintf(stderr, "[PlayMode] ERROR reverting snapshot: %s\n", e.what());
        }
    }

    // Restore editor camera.
    *world.GetActiveCamera() = editorCamera;

    std::printf("[PlayMode] ■ Stopped (revert=%s).\n", revert ? "true" : "false");
}

void EditorGameMode::Tick(float dt) {
    constexpr float kFpsSmoothAlpha = 0.1f;
    if (dt > 1e-6f) {
        float fps = 1.f / dt;
        session.fpsSmooth = session.fpsSmooth * (1.f - kFpsSmoothAlpha) + fps * kFpsSmoothAlpha;
    }
    session.gameplayCommandQueue.update(dt);
    SubterraAdvanceDayNight(session.dayNight, dt);
}

void EditorGameMode::DrawHUD() {
    if (!session.world || session.player == ecs::NULL_ENTITY)
        return;

    // Draw a compact vitals + hint overlay as a fixed, transparent ImGui window.
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground;

    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10.f, vp->WorkPos.y + vp->WorkSize.y - 90.f));
    ImGui::SetNextWindowSize(ImVec2(340.f, 80.f));
    ImGui::SetNextWindowBgAlpha(0.55f);

    if (ImGui::Begin("##playhud", nullptr, flags & ~ImGuiWindowFlags_NoBackground)) {
        auto *vitals = session.world->GetComponent<PlayerVitals>(session.player);
        if (vitals) {
            // HP bar
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.15f, 0.15f, 1.f));
            float hpFrac = vitals->health_max > 0.f ? vitals->health / vitals->health_max : 0.f;
            ImGui::ProgressBar(hpFrac, ImVec2(100.f, 12.f), "");
            ImGui::PopStyleColor();
            ImGui::SameLine();

            // Stamina bar
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.7f, 0.2f, 1.f));
            float stFrac = vitals->stamina_max > 0.f ? vitals->stamina / vitals->stamina_max : 0.f;
            ImGui::ProgressBar(stFrac, ImVec2(100.f, 12.f), "");
            ImGui::PopStyleColor();
            ImGui::SameLine();

            // Food bar
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.6f, 0.1f, 1.f));
            float foodFrac = vitals->food_satiety_max > 0.f
                                 ? vitals->food_satiety / vitals->food_satiety_max
                                 : 0.f;
            ImGui::ProgressBar(foodFrac, ImVec2(100.f, 12.f), "");
            ImGui::PopStyleColor();
        }

        if (!session.interactHint.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.f, 1.f, 0.4f, 1.f), "%s", session.interactHint.c_str());
        }
    }
    ImGui::End();
}

} // namespace subterra
