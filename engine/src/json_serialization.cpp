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
    j["terrain"]["tileSize"] = world.terrain.tileSize;
    j["terrain"]["tilesetPath"] = world.terrain.tilesetPath;
    for (const auto &layer : world.terrain.layers) {
      json jl;
      jl["width"] = layer.width;
      jl["height"] = layer.height;
      jl["tiles"] = layer.tiles;
      j["terrain"]["layers"].push_back(jl);
    }
  }

  return j;
}

SerializedWorld FromJson(const json &j) {
  SerializedWorld world;

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