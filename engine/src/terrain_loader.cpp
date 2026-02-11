#include "terrain_loader.h"
#include "serialization.h"
#include "json.hpp"
#include <fstream>

using json = nlohmann::json;
namespace criogenio {

Terrain2D TilemapLoader::LoadFromJSON(const std::string &path) {
  std::ifstream file(path);
  json data;
  file >> data;

  SerializedTerrain2D ser;
  ser.chunkSize = 0;
  ser.tileset.tilesetPath = data["tilesets"][0]["image"].get<std::string>();
  ser.tileset.tileSize = data["tilewidth"];
  ser.tileset.columns = data["tilesets"][0]["columns"];
  ser.tileset.rows = data["tilesets"][0]["tilecount"];

  for (auto &layer : data["layers"]) {
    if (layer["type"] != "tilelayer")
      continue;
    SerializedTileLayer sl;
    sl.width = layer["width"];
    sl.height = layer["height"];
    sl.tiles = layer["data"].get<std::vector<int>>();
    for (int &i : sl.tiles)
      i--;
    ser.layers.push_back(std::move(sl));
  }

  Terrain2D terrain;
  terrain.Deserialize(ser);
  return terrain;
}

} // namespace criogenio
