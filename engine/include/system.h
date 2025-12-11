#pragma once
#include "scene.h"
#include "components.h"
#include "animation_state.h"

namespace criogenio {
class MovementSystem {
public:
    void Update(float dt, Scene& scene) {

        auto entities = scene.GetEntitiesWith<Controller>();

        for (int id : entities) {
            auto& ctrl = scene.GetComponent<Controller>(id);
            auto& tr = scene.GetComponent<Transform>(id);
            auto& anim = scene.GetComponent<AnimationState>(id);

            float dx = 0, dy = 0;

            // Example raw movement
            if (IsKeyDown(KEY_RIGHT)) { dx += 1; anim.facing = criogenio::RIGHT; }
            if (IsKeyDown(KEY_LEFT)) { dx -= 1; anim.facing = criogenio::LEFT; }
            if (IsKeyDown(KEY_UP)) { dy -= 1; anim.facing = criogenio::UP; }
            if (IsKeyDown(KEY_DOWN)) { dy += 1; anim.facing = criogenio::DOWN; }

            if (dx != 0 || dy != 0) {
                tr.x += dx * ctrl.speed * dt;
                tr.y += dy * ctrl.speed * dt;
                anim.current = AnimState::Walk;
            }
            else {
                anim.current = AnimState::Idle;
            }
        }
    }
};

class AnimationSystem {
public:

    std::string FacingToString(criogenio::Direction d) const {
        switch (d) {
        case criogenio::UP: return "up";
        case criogenio::DOWN: return "down";
        case criogenio::LEFT: return "left";
        case criogenio::RIGHT: return "right";
        }
        return "down";
    }

    std::string BuildClipKey(const criogenio::AnimationState& st) {
        switch (st.current) {
        case AnimState::Idle:
            return "idle_" + FacingToString(st.facing);
        case AnimState::Walk:
            return "walk_" + FacingToString(st.facing);
        case AnimState::Attack:
            return "attack_" + FacingToString(st.facing);
        }
        return "idle_down";
    }

    void Update(float dt, Scene& scene) {

        auto entities = scene.GetEntitiesWith<AnimatedSprite>();

        for (int id : entities) {
            auto& sprite = scene.GetComponent<AnimatedSprite>(id);
            auto& st = scene.GetComponent<AnimationState>(id);

            if (st.current != st.previous) {
                const std::string clip = BuildClipKey(st);
                sprite.SetAnimation(clip);
            }

            st.previous = st.current;
            sprite.Update(dt);
        }
    }
};

class RenderSystem {
public:
    void Render(Scene& scene) {
		BeginMode2D(scene.maincamera);
        auto entities = scene.GetEntitiesWith<AnimatedSprite>();

        for (int id : entities) {
            auto& sprite = scene.GetComponent<AnimatedSprite>(id);
            auto& tr = scene.GetComponent<Transform>(id);

            DrawTextureRec(
                sprite.texture,
                sprite.GetFrame(),
                Vector2{ tr.x, tr.y },
                WHITE
            );
        }
		EndMode2D();
    }
};
} // namespace criogenio
