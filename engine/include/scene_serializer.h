#pragma once

#include "serialization.h"
#include "world.h"

#include <functional>
#include <unordered_map>

namespace criogenio {
class SerializedScene {};

class SceneSerializer {
public:
  using ComponentFactory = std::function<Component *(Scene &, int entityId)>;

  SceneSerializer();

  SerializedScene Serialize(Scene &scene);
  void Deserialize(Scene &scene, const SerializedScene &data);

  void RegisterComponent(const std::string &type, ComponentFactory factory);

private:
  std::unordered_map<std::string, ComponentFactory> componentFactories;
};

} // namespace criogenio
