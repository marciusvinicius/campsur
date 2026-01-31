#include "network/replication_server.h"
#include "components.h"
#include "ecs_registry.h"
#include "network/replication_client.h"
#include "network/net_messages.h"
#include "network/serialization.h"
#include "world.h"
#include <cstring>

namespace criogenio {

namespace {

void WriteSnapshotEntity(NetWriter &w, const SnapshotEntity &e) {
  w.Write(e.id);
  w.Write(static_cast<uint32_t>(e.componentMask));
  uint32_t sz = static_cast<uint32_t>(e.data.size());
  w.Write(sz);
  if (!e.data.empty())
    w.WriteBytes(e.data.data(), e.data.size());
}

void WriteSnapshot(NetWriter &w, const Snapshot &snap) {
  w.Write(static_cast<uint8_t>(MsgType::Snapshot));
  w.Write(snap.tick);
  uint32_t n = static_cast<uint32_t>(snap.entities.size());
  w.Write(n);
  for (const auto &e : snap.entities)
    WriteSnapshotEntity(w, e);
}

}  // namespace

ReplicationServer::ReplicationServer(World &world, INetworkTransport &net)
    : world(world), net(net) {}

void ReplicationServer::Update() {
  auto msgs = net.PollMessages();
  for (const auto &msg : msgs) {
    if (msg.data.size() < 1u)
      continue;
    MsgType type = static_cast<MsgType>(msg.data[0]);
    if (type == MsgType::Input && msg.data.size() >= sizeof(uint8_t) + sizeof(PlayerInput)) {
      PlayerInput input;
      std::memcpy(&input, msg.data.data() + 1, sizeof(PlayerInput));
      HandleInput(msg.from, input);
    }
  }
  BuildAndSendSnapshot();
}

void ReplicationServer::HandleInput(ConnectionId conn, const PlayerInput &input) {
  auto it = connectionToEntity.find(conn);
  ecs::EntityId entityId;
  if (it == connectionToEntity.end()) {
    entityId = world.CreateEntity("player");
    world.AddComponent<NetReplicated>(entityId);
    world.AddComponent<Transform>(entityId, 0.f, 0.f);
    world.AddComponent<Controller>(entityId, Vector2{0, 0});
    NetEntityId netId = nextNetId++;
    entityToNetId[entityId] = netId;
    netToEntity[netId] = Entity{static_cast<int>(entityId)};
    connectionToEntity[conn] = entityId;
  } else {
    entityId = it->second;
  }
  Controller *ctrl = world.GetComponent<Controller>(entityId);
  if (ctrl) {
    ctrl->velocity.x = input.move_x;
    ctrl->velocity.y = input.move_y;
  }
}

void ReplicationServer::BuildAndSendSnapshot() {
  Snapshot snap;
  snap.tick = serverTick++;

  auto entities = world.GetEntitiesWith<NetReplicated, Transform>();
  for (ecs::EntityId eid : entities) {
    NetEntityId netId;
    auto it = entityToNetId.find(eid);
    if (it == entityToNetId.end()) {
      netId = nextNetId++;
      entityToNetId[eid] = netId;
      netToEntity[netId] = Entity{static_cast<int>(eid)};
    } else {
      netId = it->second;
    }

    SnapshotEntity sent;
    sent.id = netId;
    sent.componentMask = COMPONENT_MASK_TRANSFORM;
    Transform *t = world.GetComponent<Transform>(eid);
    if (t) {
      NetWriter w;
      w.Write(t->x);
      w.Write(t->y);
      w.Write(t->rotation);
      w.Write(t->scale_x);
      w.Write(t->scale_y);
      sent.data = w.Data();
    }
    snap.entities.push_back(std::move(sent));
  }

  NetWriter buf;
  WriteSnapshot(buf, snap);
  const std::vector<uint8_t> &data = buf.Data();
  for (ConnectionId conn : net.GetConnectionIds()) {
    net.Send(conn, data.data(), data.size(), true);
  }
}

}  // namespace criogenio
