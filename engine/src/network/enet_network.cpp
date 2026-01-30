#include "network/enet_network.h"
#include "network/inetwork.h"

namespace criogenio {

EnetNetwork::EnetNetwork(bool isServer) {
  enet_initialize();

  if (isServer) {
    ENetAddress addr{};
    addr.host = ENET_HOST_ANY;
    addr.port = 7777;
    host = enet_host_create(&addr, 32, 2, 0, 0);
  } else {
    host = enet_host_create(nullptr, 1, 2, 0, 0);
  }
}

EnetNetwork::~EnetNetwork() {
  if (host)
    enet_host_destroy(host);
  enet_deinitialize();
}

void EnetNetwork::Update() {
  ENetEvent event;
  while (enet_host_service(host, &event, 0) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_RECEIVE:
      // Dispatch packet here
      enet_packet_destroy(event.packet);
      break;

    case ENET_EVENT_TYPE_CONNECT:
      // Handle connect
      break;

    case ENET_EVENT_TYPE_DISCONNECT:
      // Handle disconnect
      break;

    default:
      break;
    }
  }
}

std::vector<NetworkMessage> EnetNetwork::PollMessages() {
  auto out = std::move(inbox);
  inbox.clear();
  return out;
}

} // namespace criogenio
