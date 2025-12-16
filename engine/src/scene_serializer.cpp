#include "scene_serializer.h"
#include "components.h" // Transform, Controller, AnimatedSprite, etc.

namespace criogenio {

SceneSerializer::SceneSerializer() {
  // Register built-in components here
  RegisterComponent("Transform", [](Scene &scene, int entityId) -> Component * {
    return scene.AddComponent<Transform>(entityId);
  });

  RegisterComponent("Controller",
                    [](Scene &scene, int entityId) -> Component * {
                      return scene.AddComponent<Controller>(entityId);
                    });

  RegisterComponent("AnimatedSprite",
                    [](Scene &scene, int entityId) -> Component * {
                      return scene.AddComponent<AnimatedSprite>(entityId);
                    });

  // Add more as needed
}

void SceneSerializer::RegisterComponent(const std::string &type,
                                        ComponentFactory factory) {
  componentFactories[type] = factory;
}

} // namespace criogenio
