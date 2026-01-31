#pragma once
#include "enet/enet.h"
#include "network/inetwork.h"
#include <unordered_map>

namespace criogenio {

class EnetNetwork final : public INetwork {
public:
  EnetNetwork(bool isServer);
  ~EnetNetwork();

  void Update() override;
  std::vector<NetworkMessage> PollMessages() override;

private:
  ENetHost *host = nullptr;
  std::unordered_map<ENetPeer *, ConnectionId> peerToId;
  ConnectionId nextId = 1;
  std::vector<NetworkMessage> inbox;
};

}  // namespace criogenio
