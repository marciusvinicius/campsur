#pragma once

#include "entity.h"
#include "network/snapshot.h"
#include "world.h"

namespace criogenio {

extern std::unordered_map<NetEntityId, Entity> netToEntity;

class ReplicationClient {
public:
  ReplicationClient(World &world);

  void ApplySnapshot(const Snapshot &);
  void UpdateInterpolation(float dt);

private:
  World &world;
};

} // namespace criogenio
