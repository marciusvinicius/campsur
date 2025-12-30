#include "terrain.h"

namespace criogenio {

void Terrain3D::Update(float dt) {
  // Update 3D terrain logic if needed
}

void Terrain2D::Update(float dt) {
  // Update terrain logic if needed
}

void Terrain2D::Render(Renderer &renderer) {
  // Render the terrain lay
  if (!tileset.atlas)
    return; // Skip if tileset not loaded

  for (const auto &layer : layers) {
    for (int y = 0; y < layer.height; y++) {
      for (int x = 0; x < layer.width; x++) {
        int tileIndex = layer.tiles[y * layer.width + x];
        if (tileIndex < 0)
          continue; // Skip empty tiles

        int tileX = (tileIndex % tileset.columns) * tileset.tileSize;
        int tileY = (tileIndex / tileset.columns) * tileset.tileSize;

        Rectangle sourceRect = {static_cast<float>(tileX),
                                static_cast<float>(tileY),
                                static_cast<float>(tileset.tileSize),
                                static_cast<float>(tileset.tileSize)};
        Vector2 position = {static_cast<float>(x * tileset.tileSize),
                            static_cast<float>(y * tileset.tileSize)};
        renderer.DrawTextureRec(tileset.atlas->texture, sourceRect, position,
                                WHITE);
      }
    }
  }
}

void Terrain3D ::Render(Renderer &renderer) {
  // Render the 3D terrain model
  DrawModel(model, Vector3{0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
}

Terrain2D &Terrain2D::SetTile(int layerIndex, int x, int y, int tileId) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return *this;

  TileLayer &layer = layers[layerIndex];
  if (x < 0 || x >= layer.width || y < 0 || y >= layer.height)
    return *this;

  layer.tiles[y * layer.width + x] = tileId;
  return *this;
}

void Terrain2D::FillLayer(int layerIndex, int tileId) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;

  TileLayer &layer = layers[layerIndex];
  std::fill(layer.tiles.begin(), layer.tiles.end(), tileId);
}

void Terrain2D::ResizeLayer(int layerIndex, int newWidth, int newHeight) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;

  TileLayer &layer = layers[layerIndex];
  std::vector<int> newTiles(newWidth * newHeight, -1);

  for (int y = 0; y < std::min(layer.height, newHeight); y++) {
    for (int x = 0; x < std::min(layer.width, newWidth); x++) {
      newTiles[y * newWidth + x] = layer.tiles[y * layer.width + x];
    }
  }

  layer.width = newWidth;
  layer.height = newHeight;
  layer.tiles = std::move(newTiles);
}

void Terrain2D::AddLayer(int width, int height, int defaultTileId) {
  TileLayer newLayer;
  newLayer.width = width;
  newLayer.height = height;
  newLayer.tiles.resize(width * height, defaultTileId);
  layers.push_back(std::move(newLayer));
}

void Terrain2D::RemoveLayer(int layerIndex) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;

  layers.erase(layers.begin() + layerIndex);
}

void Terrain2D::ClearLayer(int layerIndex) {
  if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
    return;

  TileLayer &layer = layers[layerIndex];
  std::fill(layer.tiles.begin(), layer.tiles.end(), -1);
}

void Terrain2D::ClearAllLayers() { layers.clear(); }

void Terrain2D::SetTileset(const Tileset &newTileset) { tileset = newTileset; }

void Terrain2D::SetTileSize(int newTileSize) { tileset.tileSize = newTileSize; }

SerializedTerrain2D Terrain2D::Serialize() const {
  SerializedTerrain2D out;
  out.tileset.tileSize = tileset.tileSize;
  out.tileset.tilesetPath = tileset.tilesetPath;
  out.tileset.columns = tileset.columns;
  out.tileset.rows = tileset.rows;

  auto layersData = std::vector<SerializedTileLayer>();
  for (const auto &layer : layers) {
    SerializedTileLayer layerData;
    layerData.width = layer.width;
    layerData.height = layer.height;
    layerData.tiles = layer.tiles;
    layersData.push_back(layerData);
  }
  out.layers = layersData;

  return out;
}

void Terrain2D::Deserialize(const SerializedTerrain2D &data) {
  tileset.tileSize = data.tileset.tileSize;
  tileset.tilesetPath = data.tileset.tilesetPath;
  tileset.atlas = criogenio::AssetManager::instance().load<TextureResource>(
      tileset.tilesetPath);
  tileset.columns = data.tileset.columns;
  tileset.rows = data.tileset.rows;

  layers.clear();
  for (const auto &layerData : data.layers) {
    TileLayer layer;
    layer.width = layerData.width;
    layer.height = layerData.height;
    layer.tiles = layerData.tiles;
    layers.push_back(layer);
  }
}

} // namespace criogenio
