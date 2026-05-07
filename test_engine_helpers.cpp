#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#include "components.h"
#include "criogenio_io.h"
#include "map_authoring_components.h"
#include "network/replication_client.h"
#include "network/replication_server.h"
#include "object_layer.h"
#include "terrain.h"
#include "world.h"

using namespace criogenio;

int main() {
  std::cout << "=== Engine Helper Regression Test ===" << std::endl;

  // Bounds helper fallback path.
  {
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
    ComputeMovementBoundsPx(nullptr, 16.f, 24.f, 40, 20, 32, 32, &minX, &minY, &maxX, &maxY);
    assert(minX == 0.f && minY == 0.f);
    assert(maxX == (39.f * 32.f) - 16.f);
    assert(maxY == (19.f * 32.f) - 24.f);
  }

  World world;
  const ecs::EntityId player = world.CreateEntity("player");
  world.AddComponent<Transform>(player, 100.f, 50.f);
  world.AddComponent<ReplicatedNetId>(player, ReplicatedNetId(7));

  // Net-id lookup helper.
  {
    const ecs::EntityId found = world.FindEntityByReplicatedNetId(7);
    assert(found == player);
    assert(world.FindEntityByReplicatedNetId(999) == ecs::NULL_ENTITY);
  }

  // Camera follow helper with deadzone and smoothing behavior.
  {
    Camera2D cam{};
    cam.target = {0.f, 0.f};
    CameraFollow2DConfig cfg{};
    cfg.deadzoneHalfWidth = 10.f;
    cfg.deadzoneHalfHeight = 5.f;
    cfg.smoothingSpeed = 0.f;
    const bool ok = UpdateCameraFollow2D(world, &cam, player, 20.f, 10.f, 0.016f, cfg);
    assert(ok);
    assert(cam.target.x == 100.f);
    assert(cam.target.y == 50.f);

    cfg.smoothingSpeed = 5.f;
    world.GetComponent<Transform>(player)->x = 220.f;
    world.GetComponent<Transform>(player)->y = 90.f;
    const float prevX = cam.target.x;
    const float prevY = cam.target.y;
    const bool okSmooth = UpdateCameraFollow2D(world, &cam, player, 20.f, 10.f, 0.1f, cfg);
    assert(okSmooth);
    assert(cam.target.x > prevX);
    assert(cam.target.y > prevY);
  }

  // Delta replication behavior: unchanged transforms are skipped between full snapshots.
  {
    struct MockTransport final : INetworkTransport {
      struct SentPacket {
        ConnectionId to = 0;
        std::vector<uint8_t> data;
        bool reliable = false;
      };
      std::vector<ConnectionId> connections{1};
      std::vector<NetworkMessage> inbox;
      std::vector<SentPacket> sent;
      bool StartServer(uint16_t) override { return true; }
      bool ConnectToServer(const char *, uint16_t) override { return true; }
      void Update() override {}
      void Send(ConnectionId to, const uint8_t *data, size_t size, bool reliable) override {
        SentPacket p;
        p.to = to;
        p.reliable = reliable;
        p.data.assign(data, data + size);
        sent.push_back(std::move(p));
      }
      std::vector<NetworkMessage> PollMessages() override {
        std::vector<NetworkMessage> out;
        out.swap(inbox);
        return out;
      }
      std::vector<ConnectionId> GetConnectionIds() const override { return connections; }
      void ClearSent() { sent.clear(); }
    };

    World w;
    MockTransport net;
    ReplicationServer server(w, net);
    server.SetQuantizedTransformPayloadEnabled(true);

    server.Update(); // tick 0 -> full snapshot
    assert(!net.sent.empty());
    Snapshot snap0 = ParseSnapshotFromWire(net.sent.back().data.data(), net.sent.back().data.size());
    assert(!snap0.entities.empty());
    const auto statsAfterFirst = server.GetStats();
    assert(statsAfterFirst.entitiesSerialized > 0);
    net.ClearSent();

    server.Update(); // tick 1 -> delta snapshot
    assert(net.sent.empty());
    const auto statsAfterSecond = server.GetStats();
    assert(statsAfterSecond.entitiesDeltaSkipped >= 1);
    assert(statsAfterSecond.snapshotsSkippedNoop >= 1);
    net.ClearSent();

    const ecs::EntityId remotePlayer = w.FindEntityByReplicatedNetId(1);
    assert(remotePlayer != ecs::NULL_ENTITY);
    Transform *tr = w.GetComponent<Transform>(remotePlayer);
    assert(tr);
    tr->x += 8.f; // mark changed

    server.Update(); // tick 2 -> delta includes moved entity
    assert(!net.sent.empty());
    Snapshot snap2 = ParseSnapshotFromWire(net.sent.back().data.data(), net.sent.back().data.size());
    bool sawNetId1 = false;
    bool sawMaskedXOnly = false;
    float encodedDeltaX = 0.f;
    for (const auto &e : snap2.entities) {
      if (e.id == 1 && (e.componentMask & COMPONENT_MASK_TRANSFORM) != 0) {
        sawNetId1 = true;
        if ((e.data.size() == (sizeof(uint8_t) + sizeof(float)) ||
             e.data.size() == (sizeof(uint8_t) + sizeof(int16_t))) &&
            (e.data[0] & TRANSFORM_FIELDS_ALL) == TRANSFORM_FIELD_X) {
          sawMaskedXOnly = true;
          if ((e.data[0] & TRANSFORM_PAYLOAD_QUANTIZED) != 0) {
            int16_t q = 0;
            std::memcpy(&q, e.data.data() + 1, sizeof(int16_t));
            encodedDeltaX = static_cast<float>(q) / TRANSFORM_QUANT_POS_SCALE;
          } else {
            std::memcpy(&encodedDeltaX, e.data.data() + 1, sizeof(float));
          }
        }
      }
    }
    assert(sawNetId1);
    assert(sawMaskedXOnly);

    // Client-side compatibility: apply full + masked delta and ensure untouched fields stay intact.
    World clientWorld;
    ReplicationClient client(clientWorld);
    client.ApplySnapshot(snap0);
    client.ApplySnapshot(snap2);
    const ecs::EntityId clientEntity = clientWorld.FindEntityByReplicatedNetId(1);
    assert(clientEntity != ecs::NULL_ENTITY);
    Transform *clientTr = clientWorld.GetComponent<Transform>(clientEntity);
    assert(clientTr);
    assert(std::fabs(clientTr->x - encodedDeltaX) < 1e-4f);
    assert(clientTr->y == 0.f);
    assert(clientTr->rotation == 0.f);
  }

  // Map authoring components round-trip (Serialize / Deserialize).
  {
    World w;
    const ecs::EntityId ez = w.CreateEntity("zone");
    w.AddComponent<Transform>(ez, 10.f, 20.f);
    auto &mz = w.AddComponent<MapEventZone2D>(ez);
    mz.storage_key = "door_a";
    mz.width = 48.f;
    mz.height = 32.f;
    mz.event_trigger = "enter";
    mz.gameplay_actions = "{\"x\":1}";
    mz.activated = true;

    const ecs::EntityId ei = w.CreateEntity("inter");
    w.AddComponent<Transform>(ei, 5.f, 6.f);
    auto &iz = w.AddComponent<InteractableZone2D>(ei);
    iz.width = 24.f;
    iz.height = 24.f;
    iz.interactable_type = "chest";
    iz.tiled_object_id = 99;
    iz.properties_json = "{\"loot\":\"gold\"}";

    const ecs::EntityId es = w.CreateEntity("spawn");
    w.AddComponent<Transform>(es, 100.f, 200.f);
    auto &sz = w.AddComponent<WorldSpawnPrefab2D>(es);
    sz.prefab_name = "coin";
    sz.quantity = 3;
    sz.width = 16.f;
    sz.height = 16.f;

    const SerializedWorld sw = w.Serialize(".");
    World w2;
    w2.Deserialize(sw, ".");

    int nMap = 0, nInter = 0, nSp = 0;
    for (ecs::EntityId id : w2.GetAllEntities()) {
      if (w2.GetComponent<MapEventZone2D>(id))
        ++nMap;
      if (w2.GetComponent<InteractableZone2D>(id))
        ++nInter;
      if (w2.GetComponent<WorldSpawnPrefab2D>(id))
        ++nSp;
    }
    assert(nMap == 1 && nInter == 1 && nSp == 1);

    for (ecs::EntityId id : w2.GetAllEntities()) {
      if (auto *z = w2.GetComponent<MapEventZone2D>(id)) {
        if (z->storage_key != "door_a")
          continue;
        auto *tr = w2.GetComponent<Transform>(id);
        assert(tr && tr->x == 10.f && tr->y == 20.f);
        assert(z->width == 48.f && z->height == 32.f);
        assert(z->event_trigger == "enter");
        assert(z->gameplay_actions == "{\"x\":1}");
        assert(z->activated);
      }
      if (auto *z = w2.GetComponent<InteractableZone2D>(id)) {
        if (z->interactable_type != "chest")
          continue;
        assert(z->width == 24.f && z->tiled_object_id == 99);
        assert(z->properties_json.find("gold") != std::string::npos);
      }
      if (auto *z = w2.GetComponent<WorldSpawnPrefab2D>(id)) {
        if (z->prefab_name != "coin")
          continue;
        assert(z->quantity == 3 && z->width == 16.f);
      }
    }
  }

  // ---- Sprite::atlasPath round-trip + LayerMembership persistence ----
  // Regression: previously Sprite serialized only textureId/spriteX/Y/Size,
  // dropping atlasPath; sprites added via the editor's drag-drop path lost
  // their texture on save/reload. Object-layer authoring also relies on
  // LayerMembership round-tripping through the world serializer.
  {
    World w;
    const ecs::EntityId e = w.CreateEntity("dude");
    w.AddComponent<Name>(e, "dude");
    w.AddComponent<Transform>(e, 200.f, 150.f);
    auto &s = w.AddComponent<Sprite>(e);
    s.atlasPath = "top_down_demo/assets/sprites/dude_monster.png";
    s.spriteX = 0;
    s.spriteY = 0;
    s.spriteSize = 32;
    auto &lm = w.AddComponent<LayerMembership>(e);
    lm.layerName = "Enemies";

    const std::string outPath = "/tmp/_campsur_helpers_atlaspath.campsurlevel";
    assert(SaveWorldToFile(w, outPath));

    World w2;
    assert(LoadWorldFromFile(w2, outPath));
    int seen = 0;
    for (ecs::EntityId id : w2.GetEntitiesWith<Sprite>()) {
      auto *sp = w2.GetComponent<Sprite>(id);
      assert(sp);
      assert(sp->atlasPath == "top_down_demo/assets/sprites/dude_monster.png");
      assert(sp->spriteSize == 32);
      auto *layer = w2.GetComponent<LayerMembership>(id);
      assert(layer);
      assert(layer->layerName == "Enemies");
      ++seen;
    }
    assert(seen == 1);
  }

  std::cout << "=== Engine Helper Regression Test Passed ===" << std::endl;
  return 0;
}
