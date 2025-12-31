#pragma once

#include "components.h"
#include "functional"
#include "string.h"
#include "world.h"

namespace criogenio {

using ComponentFactoryFn = std::function<Component *(World &, int)>;

class ComponentFactory {
public:
  static void Register(const std::string &type, ComponentFactoryFn fn);
  static Component *Create(const std::string &type, World &world, int entity);

private:
  static std::unordered_map<std::string, ComponentFactoryFn> &Registry();
};

} // namespace criogenio
