#pragma once
#include "network/transport.h"
#include <enet/enet.h>
#include <unordered_map>

namespace criogenio {

class ENetTransport : public INetworkTransport {
public:
  ENetTransport();
  ~ENetTransport();

  bool StartServer(uint16_t port) override;
  bool ConnectToServer(const char *host, uint16_t port) override;

  void Update() override;

  void Send(ConnectionId to, const uint8_t *data, size_t size,
            bool reliable) override;

  std::vector<NetworkMessage> PollMessages() override;

private:
  ENetHost *host = nullptr;
  ENetPeer *serverPeer = nullptr;

  std::unordered_map<ENetPeer *, ConnectionId> peerToId;
  std::unordered_map<ConnectionId, ENetPeer *> idToPeer;

  ConnectionId nextId = 1;

  std::vector<NetworkMessage> inbox;
};

} // namespace criogenio
