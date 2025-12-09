#pragma once
#include "components.h"
#include <string>

// TODO(maraujo) Think about how to make this pointers, maybe Entity should be a
// ID and I have a array of components

namespace criogenio {
class Entity {
public:
  int id;
  std::string name;
  Transform transform;
  Sprite sprite;
  Collider collider;
  bool hasCollider = false;
  Entity(int id, const std::string &name);
};

} // namespace criogenio
