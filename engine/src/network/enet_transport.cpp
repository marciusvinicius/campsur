#include "network/enet_transport.h"
#include <cstring>

namespace criogenio {

ENetTransport::ENetTransport() { enet_initialize(); }

ENetTransport::~ENetTransport() {
  if (host)
    enet_host_destroy(host);
  enet_deinitialize();
}

bool ENetTransport::StartServer(uint16_t port) {
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = port;

  host = enet_host_create(&address, 32, 2, 0, 0);
  return host != nullptr;
}

bool ENetTransport::ConnectToServer(const char *hostName, uint16_t port) {
  host = enet_host_create(nullptr, 1, 2, 0, 0);
  if (!host)
    return false;

  ENetAddress address;
  enet_address_set_host(&address, hostName);
  address.port = port;

  serverPeer = enet_host_connect(host, &address, 2, 0);
  return serverPeer != nullptr;
}

void ENetTransport::Update() {
  ENetEvent event;
  while (enet_host_service(host, &event, 0) > 0) {
    switch (event.type) {

    case ENET_EVENT_TYPE_CONNECT: {
      ConnectionId id = nextId++;
      peerToId[event.peer] = id;
      idToPeer[id] = event.peer;
      break;
    }

    case ENET_EVENT_TYPE_RECEIVE: {
      inbox.push_back({peerToId[event.peer],
                       {event.packet->data,
                        event.packet->data + event.packet->dataLength}});
      enet_packet_destroy(event.packet);
      break;
    }

    case ENET_EVENT_TYPE_DISCONNECT: {
      ConnectionId id = peerToId[event.peer];
      peerToId.erase(event.peer);
      idToPeer.erase(id);
      break;
    }
    }
  }
}

void ENetTransport::Send(ConnectionId to, const uint8_t *data, size_t size,
                         bool reliable) {
  auto it = idToPeer.find(to);
  if (it == idToPeer.end())
    return;

  ENetPacket *packet =
      enet_packet_create(data, size, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);

  enet_peer_send(it->second, 0, packet);
}

std::vector<NetworkMessage> ENetTransport::PollMessages() {
  auto msgs = inbox;
  inbox.clear();
  return msgs;
}
} // namespace criogenio