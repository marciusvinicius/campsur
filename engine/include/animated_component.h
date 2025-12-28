#pragma once

#include "animation_state.h"
#include "components.h"
#include "json.hpp"
#include "serialization.h"

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
  struct Animation {
    std::vector<Rectangle> frames;
    float frameSpeed = 0.2f;
  };

  std::unordered_map<std::string, Animation> animations;
  std::string currentAnim = "";
  float timer = 0.0f;
  int frameIndex = 0;

  Texture2D texture;
  std::string texturePath;

  AnimatedSprite() = default;
  AnimatedSprite(const std::string &path) : texturePath(path) {}
  AnimatedSprite(const std::string &initialAnim,
                 const std::vector<Rectangle> &frames, float speed,
                 Texture2D tex, const std::string &path)
      : texture(tex), texturePath(path) {
    AddAnimation(initialAnim, frames, speed);
    SetAnimation(initialAnim);
  }

  void AddAnimation(const std::string &name,
                    const std::vector<Rectangle> &frames, float speed) {
    animations[name] = Animation{frames, speed};
  }

  void SetAnimation(const std::string &name) {
    if (currentAnim == name)
      return;
    currentAnim = name;
    frameIndex = 0;
    timer = 0.0f;
  }

  void Update(float dt) {
    if (animations.empty() || currentAnim.empty())
      return;
    auto it = animations.find(currentAnim);
    if (it == animations.end())
      return;

    auto &anim = it->second;
    if (anim.frames.empty())
      return;

    timer += dt;
    if (timer >= anim.frameSpeed) {
      timer = 0;
      frameIndex = (frameIndex + 1) % anim.frames.size();
    }
  }

  Rectangle GetFrame() const {
    const auto &anim = animations.at(currentAnim);
    if (anim.frames.empty()) {
      TraceLog(LOG_ERROR, "AnimatedSprite: animation '%s' has 0 frames!",
               currentAnim.c_str());
      return {0, 0, 0, 0}; // prevent crash
    }
    return anim.frames[frameIndex];
  }

  std::string TypeName() const override { return "AnimatedSprite"; }

  SerializedComponent Serialize() const override {
    SerializedComponent out;
    out.type = "AnimatedSprite";

    out.fields["texturePath"] = texturePath;
    out.fields["currentAnim"] = currentAnim;

    // Serialize animations into JSON string
    nlohmann::json animJson = nlohmann::json::array();
    for (const auto &[name, anim] : animations) {
      nlohmann::json a;
      a["name"] = name;
      a["frameSpeed"] = anim.frameSpeed;

      a["frameWidth"] = anim.frames[0].width;
      a["frameHeight"] = anim.frames[0].height;

      std::vector<nlohmann::json> frames;

      for (const Rectangle &r : anim.frames) {
        frames.push_back(
            {{"x", (int)(r.x / r.width)}, {"y", (int)(r.y / r.height)}});
      }

      a["frames"] = frames;
      animJson.push_back(a);
    }

    out.fields["animations"] = animJson.dump(); // store as string
    return out;
  }

  void Deserialize(const SerializedComponent &data) {
    texturePath = std::get<std::string>(data.fields.at("texturePath"));
    currentAnim = std::get<std::string>(data.fields.at("currentAnim"));
    texture = LoadTexture(texturePath.c_str());

    animations.clear();

    // constexpr int FRAME_WIDTH = 32;
    // constexpr int FRAME_HEIGHT = 32;

    // Read animation JSON
    auto animStr = std::get<std::string>(data.fields.at("animations"));
    auto animJson = nlohmann::json::parse(animStr);

    for (const auto &a : animJson) {
      Animation anim;
      anim.frameSpeed = a["frameSpeed"].get<float>();

      int fw = a["frameWidth"].get<int>();
      int fh = a["frameHeight"].get<int>();

      for (const auto &f : a["frames"]) {
        Rectangle r;
        r.x = f["x"].get<int>() * fw;
        r.y = f["y"].get<int>() * fh;
        r.width = fw;
        r.height = fh;
        anim.frames.push_back(r);
      }

      animations[a["name"].get<std::string>()] = anim;
    }
    frameIndex = 0;
    timer = 0.0f;
  }

  SerializedAnimation SerializeAnimation(const std::string &name,
                                         const Animation &anim,
                                         int frameWidth) {
    SerializedAnimation out;
    out.name = name;
    out.frameSpeed = anim.frameSpeed;
    out.frameIndices.clear();

    for (const Rectangle &r : anim.frames) {
      int index = (int)(r.x / frameWidth);
      out.frameIndices.push_back(index);
    }

    return out;
  }

  Animation DeserializeAnimation(const SerializedAnimation &data,
                                 int frameWidth, int frameHeight) {
    Animation anim;
    anim.frameSpeed = data.frameSpeed;

    for (int index : data.frameIndices) {
      Rectangle r;
      r.x = index * frameWidth;
      r.y = 0;
      r.width = frameWidth;
      r.height = frameHeight;
      anim.frames.push_back(r);
    }
    return anim;
  }
};
} // namespace criogenio