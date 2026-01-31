#include "network/replication_client.h"
#include "components.h"
#include "ecs_core.h"
#include "network/net_messages.h"
#include "network/serialization.h"
#include "world.h"
#include <stdexcept>

namespace criogenio {

std::unordered_map<NetEntityId, Entity> netToEntity;

Snapshot ParseSnapshotFromWire(const uint8_t *data, size_t size) {
  if (size < 1u + sizeof(uint32_t) * 2u)
    return {};
  NetReader r(data, size);
  uint8_t msgType = r.Read<uint8_t>();
  if (msgType != static_cast<uint8_t>(MsgType::Snapshot))
    return {};
  Snapshot snap;
  snap.tick = r.Read<uint32_t>();
  uint32_t numEntities = r.Read<uint32_t>();
  for (uint32_t i = 0; i < numEntities; ++i) {
    if (r.End())
      break;
    SnapshotEntity e;
    e.id = r.Read<NetEntityId>();
    e.componentMask = r.Read<uint32_t>();
    uint32_t dataSz = r.Read<uint32_t>();
    if (dataSz > 0) {
      if (r.Tell() + dataSz > size)
        break;
      e.data.resize(dataSz);
      r.ReadBytes(e.data.data(), dataSz);
    }
    snap.entities.push_back(std::move(e));
  }
  return snap;
}

ReplicationClient::ReplicationClient(World &world) : world(world) {}

void ReplicationClient::ApplySnapshot(const Snapshot &snap) {
  for (const auto &sent : snap.entities) {
    ecs::EntityId eid;
    auto it = netToEntity.find(sent.id);
    if (it == netToEntity.end()) {
      eid = world.CreateEntity("");
      world.AddComponent<NetReplicated>(eid);
      world.AddComponent<Transform>(eid, 0.f, 0.f);
      world.AddComponent<ReplicatedNetId>(eid, ReplicatedNetId(sent.id));
      netToEntity[sent.id] = Entity{static_cast<int>(eid)};
    } else {
      eid = static_cast<ecs::EntityId>(it->second.id);
    }
    if ((sent.componentMask & COMPONENT_MASK_TRANSFORM) != 0 &&
        sent.data.size() >= 5 * sizeof(float)) {
      NetReader r(sent.data.data(), sent.data.size());
      float x = r.Read<float>();
      float y = r.Read<float>();
      float rot = r.Read<float>();
      float sx = r.Read<float>();
      float sy = r.Read<float>();
      Transform *t = world.GetComponent<Transform>(eid);
      if (t) {
        t->x = x;
        t->y = y;
        t->rotation = rot;
        t->scale_x = sx;
        t->scale_y = sy;
      }
    }
  }
}

void ReplicationClient::UpdateInterpolation(float /*dt*/) {
  /* Optional: interpolate between last two snapshots; for now no-op. */
}

}  // namespace criogenio
