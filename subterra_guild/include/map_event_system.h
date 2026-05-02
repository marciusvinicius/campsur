#pragma once

#include "render.h"
#include "systems.h"

namespace subterra {

struct SubterraSession;

class MapEventSystem : public criogenio::ISystem {
public:
  SubterraSession *session = nullptr;
  explicit MapEventSystem(SubterraSession &s) : session(&s) {}
  void Update(float dt) override;
  void Render(criogenio::Renderer &renderer) override;
};

} // namespace subterra
