#pragma once

#include "components.h"
#include "serialization.h"
#include <string>
#include <vector>

namespace subterra {

struct SubterraSession;

struct ItemLightEmitterEntry {
  std::string source_item_prefab;
  float radius = 128.f;
  float intensity = 1.f;
  unsigned char r = 255;
  unsigned char g = 180;
  unsigned char b = 80;
};

class ItemLightEmitterState : public criogenio::Component {
public:
  bool enabled = true;
  std::string source_kind;
  std::vector<ItemLightEmitterEntry> emitters;

  std::string TypeName() const override { return "ItemLightEmitterState"; }
  criogenio::SerializedComponent Serialize() const override;
  void Deserialize(const criogenio::SerializedComponent &data) override;
};

void SubterraItemLightSyncTick(SubterraSession &session, float dt);

} // namespace subterra
