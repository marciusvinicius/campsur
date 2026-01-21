#pragma once
#include "enet/enet.h"
#include "network/inetwork.h"

namespace criogenio {

class EnetNetwork final : public INetwork {
public:
  EnetNetwork(bool isServer);
  ~EnetNetwork();

  void Update() override;
  std::vector<NetworkMessage> PollMessages() override;

private:
  ENetHost *host = nullptr;
  std::vector<NetworkMessage> inbox;
};

} // namespace criogenio
