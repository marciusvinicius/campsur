#include "network/replication_server.h"
#include "components.h"
#include "ecs_registry.h"
#include "graphics_types.h"
#include "network/replication_client.h"
#include "network/net_messages.h"
#include "network/serialization.h"
#include "world.h"
#include <cmath>
#include <cstring>
#include <limits>

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

bool CanQuantizeValue(float v, float scale) {
  const float q = v * scale;
  return q >= static_cast<float>(std::numeric_limits<int16_t>::min()) &&
         q <= static_cast<float>(std::numeric_limits<int16_t>::max());
}

int16_t QuantizeValue(float v, float scale) {
  return static_cast<int16_t>(std::round(v * scale));
}

}  // namespace

ReplicationServer::ReplicationServer(World &world, INetworkTransport &net)
    : world(world), net(net) {}

bool ReplicationServer::ShouldSerializeTransform(ecs::EntityId eid, const Transform &t, bool forceFull) {
  return BuildTransformFieldMask(eid, t, forceFull) != 0;
}

uint8_t ReplicationServer::BuildTransformFieldMask(ecs::EntityId eid, const Transform &t,
                                                   bool forceFull) const {
  if (forceFull)
    return TRANSFORM_FIELDS_ALL;
  auto it = lastSentTransform_.find(eid);
  if (it == lastSentTransform_.end())
    return TRANSFORM_FIELDS_ALL;
  const SerializedTransform &prev = it->second;
  constexpr float kEps = 1e-4f;
  uint8_t mask = 0;
  if (std::fabs(prev.x - t.x) > kEps)
    mask |= TRANSFORM_FIELD_X;
  if (std::fabs(prev.y - t.y) > kEps)
    mask |= TRANSFORM_FIELD_Y;
  if (std::fabs(prev.rotation - t.rotation) > kEps)
    mask |= TRANSFORM_FIELD_ROTATION;
  if (std::fabs(prev.scaleX - t.scale_x) > kEps)
    mask |= TRANSFORM_FIELD_SCALE_X;
  if (std::fabs(prev.scaleY - t.scale_y) > kEps)
    mask |= TRANSFORM_FIELD_SCALE_Y;
  return mask;
}

void ReplicationServer::RememberSerializedTransform(ecs::EntityId eid, const Transform &t) {
  lastSentTransform_[eid] = SerializedTransform{t.x, t.y, t.rotation, t.scale_x, t.scale_y};
}

bool ReplicationServer::CanQuantizeTransformFields(uint8_t fieldMask, const Transform &t) const {
  if ((fieldMask & TRANSFORM_FIELD_X) != 0 &&
      !CanQuantizeValue(t.x, TRANSFORM_QUANT_POS_SCALE))
    return false;
  if ((fieldMask & TRANSFORM_FIELD_Y) != 0 &&
      !CanQuantizeValue(t.y, TRANSFORM_QUANT_POS_SCALE))
    return false;
  if ((fieldMask & TRANSFORM_FIELD_ROTATION) != 0 &&
      !CanQuantizeValue(t.rotation, TRANSFORM_QUANT_ROTATION_SCALE))
    return false;
  if ((fieldMask & TRANSFORM_FIELD_SCALE_X) != 0 &&
      !CanQuantizeValue(t.scale_x, TRANSFORM_QUANT_SCALE_SCALE))
    return false;
  if ((fieldMask & TRANSFORM_FIELD_SCALE_Y) != 0 &&
      !CanQuantizeValue(t.scale_y, TRANSFORM_QUANT_SCALE_SCALE))
    return false;
  return true;
}

void ReplicationServer::Update() {
  // Ensure server has its own player entity (net id 0); created once
  if (serverPlayerEntityId == ecs::NULL_ENTITY) {
    serverPlayerEntityId = world.CreateEntity("server_player");
    world.AddComponent<NetReplicated>(serverPlayerEntityId);
    world.AddComponent<Transform>(serverPlayerEntityId, 0.f, 0.f);
    world.AddComponent<Controller>(serverPlayerEntityId, Vec2{0, 0});
    world.AddComponent<ReplicatedNetId>(serverPlayerEntityId, ReplicatedNetId(kServerPlayerNetId));
    entityToNetId[serverPlayerEntityId] = kServerPlayerNetId;
  }

  // Spawn a player for any newly connected client; use connection id as net id (connection order = color order)
  for (ConnectionId conn : net.GetConnectionIds()) {
    if (connectionToEntity.find(conn) != connectionToEntity.end())
      continue;
    ecs::EntityId entityId = world.CreateEntity("player");
    world.AddComponent<NetReplicated>(entityId);
    world.AddComponent<Transform>(entityId, 0.f, 0.f);
    world.AddComponent<Controller>(entityId, Vec2{0, 0});
    NetEntityId netId = static_cast<NetEntityId>(conn);
    world.AddComponent<ReplicatedNetId>(entityId, ReplicatedNetId(netId));
    entityToNetId[entityId] = netId;
    connectionToEntity[conn] = entityId;
  }

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
  if (it == connectionToEntity.end())
    return;  // Player created on connect; should exist by first input
  ecs::EntityId entityId = it->second;
  Controller *ctrl = world.GetComponent<Controller>(entityId);
  if (ctrl) {
    ctrl->velocity.x = input.move_x;
    ctrl->velocity.y = input.move_y;
  }
}

void ReplicationServer::SetServerPlayerInput(const PlayerInput &input) {
  if (serverPlayerEntityId == ecs::NULL_ENTITY)
    return;
  Controller *ctrl = world.GetComponent<Controller>(serverPlayerEntityId);
  if (ctrl) {
    ctrl->velocity.x = input.move_x;
    ctrl->velocity.y = input.move_y;
  }
}

void ReplicationServer::BuildAndSendSnapshot() {
  const bool forceFull = (fullSnapshotIntervalTicks > 0) &&
                         ((serverTick % fullSnapshotIntervalTicks) == 0);
  Snapshot snap;
  snap.tick = serverTick++;
  stats_.snapshotsBuilt++;
  static thread_local std::vector<ecs::EntityId> entities;
  world.GetEntitiesWith<NetReplicated, Transform>(entities);
  snap.entities.reserve(entities.size() + 1);

  auto addEntityToSnapshot = [this, &snap, forceFull](ecs::EntityId eid, NetEntityId netId) {
    stats_.entitiesConsidered++;
    Transform *t = world.GetComponent<Transform>(eid);
    if (!t)
      return;
    const uint8_t fieldMask = BuildTransformFieldMask(eid, *t, forceFull);
    if (fieldMask == 0) {
      stats_.entitiesDeltaSkipped++;
      return;
    }
    SnapshotEntity sent;
    sent.id = netId;
    sent.componentMask = COMPONENT_MASK_TRANSFORM;
    NetWriter w(sizeof(float) * 5);
    const bool quantized = useQuantizedTransformPayload_ && CanQuantizeTransformFields(fieldMask, *t);
    const uint8_t payloadMask = quantized ? static_cast<uint8_t>(fieldMask | TRANSFORM_PAYLOAD_QUANTIZED)
                                          : fieldMask;
    w.Write(payloadMask);
    if (quantized) {
      if ((fieldMask & TRANSFORM_FIELD_X) != 0)
        w.Write(QuantizeValue(t->x, TRANSFORM_QUANT_POS_SCALE));
      if ((fieldMask & TRANSFORM_FIELD_Y) != 0)
        w.Write(QuantizeValue(t->y, TRANSFORM_QUANT_POS_SCALE));
      if ((fieldMask & TRANSFORM_FIELD_ROTATION) != 0)
        w.Write(QuantizeValue(t->rotation, TRANSFORM_QUANT_ROTATION_SCALE));
      if ((fieldMask & TRANSFORM_FIELD_SCALE_X) != 0)
        w.Write(QuantizeValue(t->scale_x, TRANSFORM_QUANT_SCALE_SCALE));
      if ((fieldMask & TRANSFORM_FIELD_SCALE_Y) != 0)
        w.Write(QuantizeValue(t->scale_y, TRANSFORM_QUANT_SCALE_SCALE));
    } else {
      if ((fieldMask & TRANSFORM_FIELD_X) != 0)
        w.Write(t->x);
      if ((fieldMask & TRANSFORM_FIELD_Y) != 0)
        w.Write(t->y);
      if ((fieldMask & TRANSFORM_FIELD_ROTATION) != 0)
        w.Write(t->rotation);
      if ((fieldMask & TRANSFORM_FIELD_SCALE_X) != 0)
        w.Write(t->scale_x);
      if ((fieldMask & TRANSFORM_FIELD_SCALE_Y) != 0)
        w.Write(t->scale_y);
    }
    sent.data = w.Data();
    snap.entities.push_back(std::move(sent));
    stats_.entitiesSerialized++;
    RememberSerializedTransform(eid, *t);
  };

  // Always send server player first (net id 0) so clients see the server's character
  if (serverPlayerEntityId != ecs::NULL_ENTITY) {
    addEntityToSnapshot(serverPlayerEntityId, kServerPlayerNetId);
  }

  for (ecs::EntityId eid : entities) {
    if (eid == serverPlayerEntityId)
      continue;  // already added above
    NetEntityId netId;
    auto it = entityToNetId.find(eid);
    if (it == entityToNetId.end()) {
      netId = nextNetId++;
      entityToNetId[eid] = netId;
    } else {
      netId = it->second;
    }
    addEntityToSnapshot(eid, netId);
  }

  const size_t approxEntityBytes =
      snap.entities.size() *
      (sizeof(NetEntityId) + sizeof(uint32_t) * 2 + sizeof(uint8_t) + sizeof(float) * 5);
  if (!forceFull && snap.entities.empty()) {
    stats_.snapshotsSkippedNoop++;
    return;
  }
  NetWriter buf(sizeof(uint8_t) + sizeof(uint32_t) * 2 + approxEntityBytes);
  WriteSnapshot(buf, snap);
  const std::vector<uint8_t> &data = buf.Data();
  for (ConnectionId conn : net.GetConnectionIds()) {
    net.Send(conn, data.data(), data.size(), true);
    stats_.snapshotsSent++;
    stats_.bytesSent += data.size();
  }
}

}  // namespace criogenio
