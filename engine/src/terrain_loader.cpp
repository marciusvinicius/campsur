
#include "terrain_loader.h"
#include "json.hpp" // nlohmann::json
#include <fstream>

using json = nlohmann::json;
namespace criogenio {

Terrain2D TilemapLoader::LoadFromJSON(const std::string &path) {
  std::ifstream file(path);
  json data;
  file >> data;

  Terrain2D terrain;

  auto &tilesetJson = data["tilesets"][0];
  std::string img = tilesetJson["image"];

  terrain.tileset.atlas = LoadTexture(img.c_str());
  terrain.tileset.tileSize = data["tilewidth"];
  terrain.tileset.columns = tilesetJson["columns"];
  terrain.tileset.rows = tilesetJson["tilecount"];

  for (auto &layer : data["layers"]) {
    if (layer["type"] != "tilelayer")
      continue;

    TileLayer t;
    t.width = layer["width"];
    t.height = layer["height"];
    t.tiles = layer["data"].get<std::vector<int>>();

    for (int &i : t.tiles)
      i--;

    terrain.layers.push_back(t);
  }

  return terrain;
}

}; // namespace criogenio
