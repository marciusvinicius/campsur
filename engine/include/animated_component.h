#pragma once

#include "animation_database.h"
#include "animation_state.h"
#include "asset_manager.h"
#include "components.h"
#include "json.hpp"
#include "resources.h"
#include "serialization.h"
#include <string>

namespace criogenio {

class AnimationState : public Component {
public:
  AnimationState() = default;
  AnimState current = AnimState::IDLE;
  AnimState previous = AnimState::IDLE;
  Direction facing = Direction::DOWN;

  std::string TypeName() const override { return "AnimationState"; }

  SerializedComponent Serialize() const override {
    return {"AnimationState",
            {
                {"current", static_cast<int>(current)},
                {"previous", static_cast<int>(previous)},
                {"facing", static_cast<int>(facing)},
            }};
  }

  void Deserialize(const SerializedComponent &data) override {
    current = static_cast<AnimState>(std::get<int>(data.fields.at("current")));
    previous =
        static_cast<AnimState>(std::get<int>(data.fields.at("previous")));
    facing = static_cast<Direction>(std::get<int>(data.fields.at("facing")));
  }
  void SetState(AnimState newState) {
    if (current != newState) {
      previous = current;
      current = newState;
    }
  }
  void SetDirection(Direction newDirection) { facing = newDirection; }
};

class AnimatedSprite : public Component {
public:
  // Runtime state only—no owned assets or frame data
  AssetId animationId = INVALID_ASSET_ID;
  std::string currentClipName;
  float timer = 0.0f;
  int frameIndex = 0;

  AnimatedSprite() = default;
  explicit AnimatedSprite(AssetId id) : animationId(id) {}

  void SetClip(const std::string &clipName) {
    if (currentClipName == clipName)
      return;
    currentClipName = clipName;
    frameIndex = 0;
    timer = 0.0f;
  }

  void Update(float dt) {
    if (animationId == INVALID_ASSET_ID || currentClipName.empty())
      return;

    const auto *clip =
        AnimationDatabase::instance().getClip(animationId, currentClipName);
    if (!clip || clip->frames.empty())
      return;

    timer += dt;
    if (timer >= clip->frameSpeed) {
      timer = 0.0f;
      frameIndex = (frameIndex + 1) % clip->frames.size();
    }
  }

  Rectangle GetFrame() const {
    if (animationId == INVALID_ASSET_ID || currentClipName.empty())
      return {0, 0, 0, 0};

    const auto *clip =
        AnimationDatabase::instance().getClip(animationId, currentClipName);
    if (!clip || clip->frames.empty()) {
      TraceLog(LOG_ERROR, "AnimatedSprite: clip '%s' not found or empty",
               currentClipName.c_str());
      return {0, 0, 0, 0};
    }
    return clip->frames[frameIndex].rect;
  }

  std::string TypeName() const override { return "AnimatedSprite"; }

  SerializedComponent Serialize() const override {
    SerializedComponent out;
    out.type = TypeName();
    out.fields["animationId"] = static_cast<int>(animationId);
    out.fields["currentClipName"] = currentClipName;
    return out;
  }

  void Deserialize(const SerializedComponent &data) override {
    animationId =
        static_cast<AssetId>(std::get<int>(data.fields.at("animationId")));
    currentClipName = std::get<std::string>(data.fields.at("currentClipName"));
    frameIndex = 0;
    timer = 0.0f;
  }
};
} // namespace criogenio