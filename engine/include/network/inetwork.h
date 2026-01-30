#pragma once
#include "network/transport.h"

namespace criogenio {

class INetwork {
public:
  virtual ~INetwork() = default;
  virtual void Update() = 0;
  virtual std::vector<NetworkMessage> PollMessages() = 0;
};

} // namespace criogenio
