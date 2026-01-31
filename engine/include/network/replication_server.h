#pragma once

#include "ecs_core.h"
#include "entity.h"
#include "network/net_entity.h"
#include "network/net_messages.h"
#include "network/snapshot.h"
#include "network/transport.h"
#include "world.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace criogenio {

class ReplicationServer {
public:
  ReplicationServer(World &world, INetworkTransport &net);

  void Update();
  void HandleInput(ConnectionId, const PlayerInput &);

private:
  void BuildAndSendSnapshot();

  World &world;
  INetworkTransport &net;
  std::unordered_map<ecs::EntityId, NetEntityId> entityToNetId;
  std::unordered_map<ConnectionId, ecs::EntityId> connectionToEntity;
  NetEntityId nextNetId = 1;
  uint32_t serverTick = 0;
};

}  // namespace criogenio
