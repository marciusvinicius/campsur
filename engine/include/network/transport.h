#pragma once
#include <cstdint>
#include <vector>

namespace criogenio {

using ConnectionId = uint32_t;

struct NetworkMessage {
  ConnectionId from;
  std::vector<uint8_t> data;
};

class INetworkTransport {
public:
  virtual ~INetworkTransport() = default;

  virtual bool StartServer(uint16_t port) = 0;
  virtual bool ConnectToServer(const char *host, uint16_t port) = 0;

  virtual void Update() = 0;

  virtual void Send(ConnectionId to, const uint8_t *data, size_t size,
                    bool reliable) = 0;

  virtual std::vector<NetworkMessage> PollMessages() = 0;
};

} // namespace criogenio
