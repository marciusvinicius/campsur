#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "entity.h"
#include "input.h"
#include "network/net_component.h"
#include "network/net_messages.h"
#include "network/snapshot.h"
#include "network/transport.h"
#include "world.h"

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
};

} // namespace criogenio
