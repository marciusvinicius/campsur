#include "network/replication_client.h"
#include "components.h"
#include "ecs_core.h"
#include "network/net_messages.h"
#include "network/serialization.h"
#include "world.h"
#include <stdexcept>

namespace criogenio {

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
    auto it = netToEntity_.find(sent.id);
    if (it == netToEntity_.end()) {
      eid = world.CreateEntity("");
      world.AddComponent<NetReplicated>(eid);
      world.AddComponent<Transform>(eid, 0.f, 0.f);
      world.AddComponent<ReplicatedNetId>(eid, ReplicatedNetId(sent.id));
      netToEntity_[sent.id] = eid;
    } else {
      eid = it->second;
    }
    if ((sent.componentMask & COMPONENT_MASK_TRANSFORM) != 0 && !sent.data.empty()) {
      Transform *t = world.GetComponent<Transform>(eid);
      if (!t)
        continue;

      // Backward compatibility: old format had exactly 5 packed floats and no field mask.
      if (sent.data.size() == 5 * sizeof(float)) {
        NetReader r(sent.data.data(), sent.data.size());
        t->x = r.Read<float>();
        t->y = r.Read<float>();
        t->rotation = r.Read<float>();
        t->scale_x = r.Read<float>();
        t->scale_y = r.Read<float>();
        continue;
      }

      NetReader r(sent.data.data(), sent.data.size());
      const uint8_t payloadMask = r.Read<uint8_t>();
      const bool quantized = (payloadMask & TRANSFORM_PAYLOAD_QUANTIZED) != 0;
      const uint8_t fieldMask = static_cast<uint8_t>(payloadMask & TRANSFORM_FIELDS_ALL);
      size_t fields = 0;
      if ((fieldMask & TRANSFORM_FIELD_X) != 0)
        ++fields;
      if ((fieldMask & TRANSFORM_FIELD_Y) != 0)
        ++fields;
      if ((fieldMask & TRANSFORM_FIELD_ROTATION) != 0)
        ++fields;
      if ((fieldMask & TRANSFORM_FIELD_SCALE_X) != 0)
        ++fields;
      if ((fieldMask & TRANSFORM_FIELD_SCALE_Y) != 0)
        ++fields;
      const size_t neededBytes =
          sizeof(uint8_t) + fields * (quantized ? sizeof(int16_t) : sizeof(float));
      if (sent.data.size() < neededBytes)
        continue;
      if (quantized) {
        if ((fieldMask & TRANSFORM_FIELD_X) != 0)
          t->x = static_cast<float>(r.Read<int16_t>()) / TRANSFORM_QUANT_POS_SCALE;
        if ((fieldMask & TRANSFORM_FIELD_Y) != 0)
          t->y = static_cast<float>(r.Read<int16_t>()) / TRANSFORM_QUANT_POS_SCALE;
        if ((fieldMask & TRANSFORM_FIELD_ROTATION) != 0)
          t->rotation = static_cast<float>(r.Read<int16_t>()) / TRANSFORM_QUANT_ROTATION_SCALE;
        if ((fieldMask & TRANSFORM_FIELD_SCALE_X) != 0)
          t->scale_x = static_cast<float>(r.Read<int16_t>()) / TRANSFORM_QUANT_SCALE_SCALE;
        if ((fieldMask & TRANSFORM_FIELD_SCALE_Y) != 0)
          t->scale_y = static_cast<float>(r.Read<int16_t>()) / TRANSFORM_QUANT_SCALE_SCALE;
      } else {
        if ((fieldMask & TRANSFORM_FIELD_X) != 0)
          t->x = r.Read<float>();
        if ((fieldMask & TRANSFORM_FIELD_Y) != 0)
          t->y = r.Read<float>();
        if ((fieldMask & TRANSFORM_FIELD_ROTATION) != 0)
          t->rotation = r.Read<float>();
        if ((fieldMask & TRANSFORM_FIELD_SCALE_X) != 0)
          t->scale_x = r.Read<float>();
        if ((fieldMask & TRANSFORM_FIELD_SCALE_Y) != 0)
          t->scale_y = r.Read<float>();
      }
    }
  }
}

void ReplicationClient::UpdateInterpolation(float /*dt*/) {
  /* Optional: interpolate between last two snapshots; for now no-op. */
}

}  // namespace criogenio
