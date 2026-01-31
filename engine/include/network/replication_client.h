#pragma once

#include "entity.h"
#include "network/snapshot.h"
#include "world.h"
#include <unordered_map>

namespace criogenio {

extern std::unordered_map<NetEntityId, Entity> netToEntity;

/** Parse snapshot from wire format (MsgType::Snapshot + tick + entities). */
Snapshot ParseSnapshotFromWire(const uint8_t *data, size_t size);

class ReplicationClient {
public:
  explicit ReplicationClient(World &world);

  void ApplySnapshot(const Snapshot &snap);
  void UpdateInterpolation(float dt);

private:
  World &world;
};

}  // namespace criogenio
