#include "terrain.h"

namespace criogenio {
void Terrain2D ::Render(Renderer &renderer) {
  // Render the terrain lay
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
        renderer.DrawTextureRec(tileset.atlas, sourceRect, position, WHITE);
      }
    }
  }
}

void Terrain3D ::Render(Renderer &renderer) {
  // Render the 3D terrain model
  DrawModel(model, Vector3{0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
}

} // namespace criogenio
