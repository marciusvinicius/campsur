#include "engine.h"
#include "asset_manager.h"
#include "component_factory.h"
#include "input.h"
#include "network/net_messages.h"
#include "network/replication_client.h"
#include "resources.h"
#include "world.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace criogenio {

static float GetTimeSeconds() {
  using namespace std::chrono;
  return duration<float>(steady_clock::now().time_since_epoch()).count();
}

Engine::Engine(int width, int height, const char* title) : width(width), height(height) {
  RegisterCoreComponents();
  renderer = new Renderer(width, height, title);
  world = new World();

  AssetManager::instance().registerLoader<TextureResource>(
      [this](const std::string& p) -> std::shared_ptr<TextureResource> {
        TextureHandle h = renderer->LoadTexture(p);
        if (!h.valid())
          return nullptr;
        return std::make_shared<TextureResource>(p, h, renderer);
      });
}

Engine::~Engine() {
  replicationClient.reset();
  replicationServer.reset();
  transport.reset();
  criogenio::AnimationDatabase::instance().clear();
  if (world) {
    delete world;
    world = nullptr;
  }
  criogenio::AssetManager::instance().clear();
  if (renderer) {
    delete renderer;
    renderer = nullptr;
  }
}

World& Engine::GetWorld() { return *world; }
EventBus& Engine::GetEventBus() { return eventBus; }
Renderer& Engine::GetRenderer() { return *renderer; }

Vec2 Engine::GetMousePosition() const {
  return renderer ? renderer->GetMousePosition() : Vec2{0, 0};
}

Vec2 Engine::GetMouseWorld() const {
  if (!renderer || !world)
    return Vec2{0, 0};
  Vec2 screen = renderer->GetMousePosition();
  return ScreenToWorld2D(screen, world->maincamera,
                        (float)renderer->GetViewportWidth(),
                        (float)renderer->GetViewportHeight());
}

bool Engine::StartServer(uint16_t port) {
  if (networkMode != NetworkMode::Off)
    return false;
  transport = std::make_unique<ENetTransport>();
  if (!transport->StartServer(port)) {
    transport.reset();
    return false;
  }
  replicationServer =
      std::make_unique<ReplicationServer>(*world, *transport);
  networkMode = NetworkMode::Server;
  return true;
}

bool Engine::ConnectToServer(const char* host, uint16_t port) {
  if (networkMode != NetworkMode::Off)
    return false;
  transport = std::make_unique<ENetTransport>();
  if (!transport->ConnectToServer(host, port)) {
    transport.reset();
    return false;
  }
  replicationClient = std::make_unique<ReplicationClient>(*world);
  networkMode = NetworkMode::Client;
  return true;
}

INetworkTransport* Engine::GetTransport() {
  return transport.get();
}

void Engine::SendInputAsClient(const PlayerInput& input) {
  if (networkMode != NetworkMode::Client || !transport)
    return;
  std::vector<ConnectionId> ids = transport->GetConnectionIds();
  if (ids.empty())
    return;
  std::vector<uint8_t> buf;
  buf.push_back(static_cast<uint8_t>(MsgType::Input));
  buf.resize(1 + sizeof(PlayerInput));
  std::memcpy(buf.data() + 1, &input, sizeof(PlayerInput));
  transport->Send(ids[0], buf.data(), buf.size(), true);
}

void Engine::SetServerPlayerInput(const PlayerInput& input) {
  if (networkMode != NetworkMode::Server || !replicationServer)
    return;
  replicationServer->SetServerPlayerInput(input);
}

void Engine::Run() {
  if (!renderer || !renderer->IsValid()) {
    std::fprintf(stderr, "Renderer failed to initialize: %s\n",
                 renderer ? renderer->GetInitError() : "no renderer");
    return;
  }
  previousTime = GetTimeSeconds();
  while (!renderer->WindowShouldClose()) {
    renderer->ProcessEvents();
    float now = GetTimeSeconds();
    float dt = now - previousTime;
    previousTime = now;

    OnFrame(dt);
    world->Update(dt);

    if (networkMode == NetworkMode::Server && transport && replicationServer) {
      transport->Update();
      replicationServer->Update();
    } else if (networkMode == NetworkMode::Client && transport &&
               replicationClient) {
      transport->Update();
      auto msgs = transport->PollMessages();
      for (const auto& msg : msgs) {
        if (msg.data.size() >= 1u &&
            static_cast<MsgType>(msg.data[0]) == MsgType::Snapshot) {
          Snapshot snap = ParseSnapshotFromWire(msg.data.data(), msg.data.size());
          replicationClient->ApplySnapshot(snap);
        }
      }
      replicationClient->UpdateInterpolation(dt);
    }
    renderer->BeginFrame();
    world->Render(*renderer);
    OnGUI();
    renderer->EndFrame();
    Input::EndFrame();
  }
}

void Engine::RegisterCoreComponents() {
  ComponentFactory::Register("Transform", [](World& w, int e) {
    return &w.AddComponent<Transform>(e);
  });
  ComponentFactory::Register("AnimatedSprite", [](World& w, int e) {
    return &w.AddComponent<AnimatedSprite>(e);
  });
  ComponentFactory::Register("Controller", [](World& w, int e) {
    return &w.AddComponent<Controller>(e);
  });
  ComponentFactory::Register("AnimationState", [](World& w, int e) {
    return &w.AddComponent<AnimationState>(e);
  });
  ComponentFactory::Register("AIController", [](World& w, int e) {
    return &w.AddComponent<AIController>(e);
  });
  ComponentFactory::Register(
      "Name", [](World& w, int e) { return &w.AddComponent<Name>(e, ""); });
  ComponentFactory::Register("NetReplicated", [](World& w, int e) {
    return &w.AddComponent<NetReplicated>(e);
  });
}

} // namespace criogenio
