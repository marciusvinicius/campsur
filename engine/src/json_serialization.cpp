#include "json_serialization.h"

namespace criogenio {

static json VariantToJson(const Variant &v) {
  return std::visit([](auto &&value) -> json { return value; }, v);
}

static Variant JsonToVariant(const json &j) {
  if (j.is_number_integer())
    return j.get<int>();
  if (j.is_number_float())
    return j.get<float>();
  if (j.is_boolean())
    return j.get<bool>();
  if (j.is_string())
    return j.get<std::string>();
  throw std::runtime_error("Unsupported JSON variant type");
}

json ToJson(const SerializedWorld &world) {
  json j;

  for (const auto &ent : world.entities) {
    json je;
    je["id"] = ent.id;

    for (const auto &comp : ent.components) {
      json jc;
      jc["type"] = comp.type;

      for (const auto &[key, value] : comp.fields) {
        std::visit([&](auto &&v) { jc["fields"][key] = v; }, value);
      }

      je["components"].push_back(jc);
    }

    j["entities"].push_back(je);
  }

  // Terrain serialization can be added here if needed
  if (world.terrain.layers.size() > 0) {
    for (const auto &layer : world.terrain.layers) {
      json jl;
      jl["width"] = layer.width;
      jl["height"] = layer.height;
      jl["tiles"] = layer.tiles;
      j["terrain"]["layers"].push_back(jl);
    }
    j["terrain"]["tileset"]["columns"] = world.terrain.tileset.columns;
    j["terrain"]["tileset"]["rows"] = world.terrain.tileset.rows;
    j["terrain"]["tileset"]["tileSize"] = world.terrain.tileset.tileSize;
    j["terrain"]["tileset"]["tilesetPath"] = world.terrain.tileset.tilesetPath;
  }

  // Serialize animations
  for (const auto &anim : world.animations) {
    json ja;
    ja["id"] = anim.id;
    ja["texturePath"] = anim.texturePath;

    for (const auto &clip : anim.clips) {
      json jc;
      jc["name"] = clip.name;
      jc["frameSpeed"] = clip.frameSpeed;
      // Optional AnimState / Direction links for this clip
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

    j["animations"].push_back(ja);
  }

  return j;
}

SerializedWorld FromJson(const json &j) {
  SerializedWorld world;

  if (j.contains("terrain")) {
    for (const auto &jl : j.at("terrain").at("layers")) {
      SerializedTileLayer layer;
      layer.width = jl.at("width").get<int>();
      layer.height = jl.at("height").get<int>();
      layer.tiles = jl.at("tiles").get<std::vector<int>>();
      world.terrain.layers.push_back(std::move(layer));
    }
    world.terrain.tileset = SerializedTileSet();
    world.terrain.tileset.columns =
        j.at("terrain").at("tileset").at("columns").get<int>();
    world.terrain.tileset.rows =
        j.at("terrain").at("tileset").at("rows").get<int>();
    world.terrain.tileset.tileSize =
        j.at("terrain").at("tileset").at("tileSize").get<int>();
    world.terrain.tileset.tilesetPath =
        j.at("terrain").at("tileset").at("tilesetPath").get<std::string>();
  }

  // Deserialize animations
  if (j.contains("animations")) {
    for (const auto &ja : j.at("animations")) {
      SerializedAnimation anim;
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
      world.animations.push_back(anim);
    }
  }

  if (j.contains("entities") == false)
    return world;

  for (const auto &je : j.at("entities")) {
    SerializedEntity ent;
    ent.id = je.at("id").get<int>();

    for (const auto &jc : je.at("components")) {
      SerializedComponent comp;
      comp.type = jc.at("type").get<std::string>();

      for (auto it = jc.at("fields").begin(); it != jc.at("fields").end();
           ++it) {

        if (it->is_number_integer())
          comp.fields[it.key()] = it->get<int>();
        else if (it->is_number_float())
          comp.fields[it.key()] = it->get<float>();
        else if (it->is_boolean())
          comp.fields[it.key()] = it->get<bool>();
        else if (it->is_string())
          comp.fields[it.key()] = it->get<std::string>();
      }
      ent.components.push_back(std::move(comp));
    }
    world.entities.push_back(std::move(ent));
  }

  return world;
}
} // namespace criogenio