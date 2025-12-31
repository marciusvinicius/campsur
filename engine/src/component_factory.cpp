#include "component_factory.h"

namespace criogenio {

std::unordered_map<std::string, ComponentFactoryFn> &
ComponentFactory::Registry() {
  static std::unordered_map<std::string, ComponentFactoryFn> registry;
  return registry;
}

void ComponentFactory::Register(const std::string &type,
                                ComponentFactoryFn fn) {
  Registry()[type] = std::move(fn);
}

Component *ComponentFactory::Create(const std::string &type, World &world,
                                    int entity) {
  auto it = Registry().find(type);
  if (it == Registry().end())
    return nullptr;

  return it->second(world, entity);
}

} // namespace criogenio