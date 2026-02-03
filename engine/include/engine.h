#pragma once

#include "core_systems.h"
#include "event.h"
#include "network/enet_transport.h"
#include "network/replication_client.h"
#include "network/replication_server.h"
#include "network/transport.h"

#include "raylib.h"
#if defined(_WIN32)
#ifdef CloseWindow
#undef CloseWindow  // Windows winuser.h conflicts with raylib's CloseWindow()
#endif
#ifdef ShowCursor
#undef ShowCursor   // Windows winuser.h conflicts with raylib's ShowCursor()
#endif
#endif
#include "render.h"
#include "world.h"
#include <memory>

namespace criogenio {

Texture2D CriogenioLoadTexture(const char *file_name);

enum class NetworkMode { Off, Server, Client };

class Engine {
public:
  int width;
  int height;
  Engine(int width, int height, const char *title);
  ~Engine();

  void Run();

  World &GetWorld();
  EventBus &GetEventBus();
  Renderer &GetRenderer();
  Vector2 GetMouseWorld();

  /** Start as network server; returns false if bind failed. */
  bool StartServer(uint16_t port);
  /** Connect as client; returns false if connect failed. */
  bool ConnectToServer(const char *host, uint16_t port);
  NetworkMode GetNetworkMode() const { return networkMode; }
  INetworkTransport *GetTransport();
  /** Send input to server (call from client each frame). */
  void SendInputAsClient(const PlayerInput &input);
  /** Apply input for the server's local player (call from server each frame when server is also a player). */
  void SetServerPlayerInput(const PlayerInput &input);

  void RegisterCoreComponents();

protected:
  virtual void OnGUI() {}
  /** Called each frame before network and world update; override to e.g. send client input. */
  virtual void OnFrame(float dt) { (void)dt; }

private:
  Renderer *renderer = nullptr;
  World *world = nullptr;
  EventBus eventBus;
  float previousTime = 0;

  NetworkMode networkMode = NetworkMode::Off;
  std::unique_ptr<ENetTransport> transport;
  std::unique_ptr<ReplicationServer> replicationServer;
  std::unique_ptr<ReplicationClient> replicationClient;
};

}  // namespace criogenio
