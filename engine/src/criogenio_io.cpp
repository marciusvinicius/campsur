#include "criogenio_io.h"
#include "json_serialization.h"
#include "asset_manager.h"
#include "resources.h"
#include <fstream>

namespace criogenio {

bool SaveWorldToFile(const World &world, const std::string &path) {
  SerializedWorld sw = world.Serialize();
  json j = ToJson(sw);

  std::ofstream file(path);
  if (!file.is_open())
    return false;

  file << j.dump(2); // pretty-print
  return true;
}

bool LoadWorldFromFile(World &world, const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return false;

  json j;
  file >> j;

  SerializedWorld sw = FromJson(j);
  world.Deserialize(sw);
  return true;
}

bool SaveAnimationToFile(AssetId animId, const std::string &path) {
  const auto *animDef = AnimationDatabase::instance().getAnimation(animId);
  if (!animDef)
    return false;

  // Build a SerializedAnimation mirroring World::Serialize logic
  SerializedAnimation serializedAnim;
  serializedAnim.id = animDef->id;
  serializedAnim.texturePath = animDef->texturePath;

  for (const auto &clip : animDef->clips) {
    SerializedAnimationClip serializedClip;
    serializedClip.name = clip.name;
    serializedClip.frameSpeed = clip.frameSpeed;
    serializedClip.state = static_cast<int>(clip.state);
    serializedClip.direction = static_cast<int>(clip.direction);

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

  // Serialize this single animation to JSON (shape matches entries in
  // world.json["animations"])
  json ja;
  ja["id"] = serializedAnim.id;
  ja["texturePath"] = serializedAnim.texturePath;

  for (const auto &clip : serializedAnim.clips) {
    json jc;
    jc["name"] = clip.name;
    jc["frameSpeed"] = clip.frameSpeed;
    jc["state"] = clip.state;
    jc["direction"] = clip.direction;

    for (const auto &frame : clip.frames) {
      json jf;
      jf["x"] = frame.x;
      jf["y"] = frame.y;
      jf["width"] = frame.width;
      jf["height"] = frame.height;
      jc["frames"].push_back(jf);
    }

    ja["clips"].push_back(jc);
  }

  std::ofstream file(path);
  if (!file.is_open())
    return false;

  file << ja.dump(2);
  return true;
}

AssetId LoadAnimationFromFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return INVALID_ASSET_ID;

  json ja;
  file >> ja;

  // Parse JSON into SerializedAnimation (same shape as in FromJson for
  // animations)
  SerializedAnimation anim;
  if (ja.contains("id"))
    anim.id = ja.at("id").get<uint32_t>();
  anim.texturePath = ja.at("texturePath").get<std::string>();

  if (ja.contains("clips")) {
    for (const auto &jc : ja.at("clips")) {
      SerializedAnimationClip clip;
      clip.name = jc.at("name").get<std::string>();
      clip.frameSpeed = jc.at("frameSpeed").get<float>();
      if (jc.contains("state")) {
        clip.state = jc.at("state").get<int>();
      }
      if (jc.contains("direction")) {
        clip.direction = jc.at("direction").get<int>();
      }

      if (jc.contains("frames")) {
        for (const auto &jf : jc.at("frames")) {
          SerializedAnimationFrame frame;
          frame.x = jf.at("x").get<float>();
          frame.y = jf.at("y").get<float>();
          frame.width = jf.at("width").get<float>();
          frame.height = jf.at("height").get<float>();
          clip.frames.push_back(frame);
        }
      }

      anim.clips.push_back(clip);
    }
  }

  if (anim.texturePath.empty())
    return INVALID_ASSET_ID;

  // Create animation in database and load texture (mirrors World::Deserialize)
  AssetId createdId =
      AnimationDatabase::instance().createAnimation(anim.texturePath);

  auto texture =
      AssetManager::instance().load<TextureResource>(anim.texturePath);
  if (!texture) {
    TraceLog(LOG_ERROR,
             "Failed to load texture for animation from file: %s (new ID: %u)",
             anim.texturePath.c_str(), createdId);
  }

  for (const auto &serializedClip : anim.clips) {
    AnimationClip clip;
    clip.name = serializedClip.name;
    clip.frameSpeed = serializedClip.frameSpeed;
    clip.state = static_cast<AnimState>(serializedClip.state);
    clip.direction = static_cast<Direction>(serializedClip.direction);

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

  return createdId;
}

} // namespace criogenio