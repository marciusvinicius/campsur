#pragma once

#include "core_systems.h"
#include "event.h"
#include "graphics_types.h"
#include "network/enet_transport.h"
#include "network/replication_client.h"
#include "network/replication_server.h"
#include "network/transport.h"
#include "render.h"
#include "world.h"
#include <memory>

namespace criogenio {

enum class NetworkMode { Off, Server, Client };

class Engine {
public:
  int width;
  int height;
  Engine(int width, int height, const char* title);
  ~Engine();

  void Run();

  World& GetWorld();
  EventBus& GetEventBus();
  Renderer& GetRenderer();
  Vec2 GetMousePosition() const;
  Vec2 GetMouseWorld() const;

  bool StartServer(uint16_t port);
  bool ConnectToServer(const char* host, uint16_t port);
  NetworkMode GetNetworkMode() const { return networkMode; }
  INetworkTransport* GetTransport();
  void SendInputAsClient(const PlayerInput& input);
  void SetServerPlayerInput(const PlayerInput& input);

  void RegisterCoreComponents();

protected:
  virtual void OnGUI() {}
  virtual void OnFrame(float dt) { (void)dt; }

private:
  Renderer* renderer = nullptr;
  World* world = nullptr;
  EventBus eventBus;
  float previousTime = 0;

  NetworkMode networkMode = NetworkMode::Off;
  std::unique_ptr<ENetTransport> transport;
  std::unique_ptr<ReplicationServer> replicationServer;
  std::unique_ptr<ReplicationClient> replicationClient;
};

} // namespace criogenio
