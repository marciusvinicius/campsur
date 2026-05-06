#pragma once

#include "ecs_core.h"
#include "entity.h"
#include "network/net_entity.h"
#include "network/net_messages.h"
#include "network/snapshot.h"
#include "network/transport.h"
#include "world.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace criogenio {

class ReplicationServer {
public:
  struct Stats {
    uint64_t snapshotsBuilt = 0;
    uint64_t snapshotsSent = 0;
    uint64_t entitiesConsidered = 0;
    uint64_t entitiesSerialized = 0;
    uint64_t entitiesDeltaSkipped = 0;
    uint64_t snapshotsSkippedNoop = 0;
    uint64_t bytesSent = 0;
  };

  ReplicationServer(World &world, INetworkTransport &net);

  void Update();
  void HandleInput(ConnectionId, const PlayerInput &);

  /** Apply input for the server's local player (when server is also a player). */
  void SetServerPlayerInput(const PlayerInput &input);
  void SetQuantizedTransformPayloadEnabled(bool enabled) {
    useQuantizedTransformPayload_ = enabled;
  }
  bool IsQuantizedTransformPayloadEnabled() const { return useQuantizedTransformPayload_; }
  const Stats &GetStats() const { return stats_; }
  void ResetStats() { stats_ = {}; }

private:
  void BuildAndSendSnapshot();
  bool ShouldSerializeTransform(ecs::EntityId eid, const Transform &t, bool forceFull);
  uint8_t BuildTransformFieldMask(ecs::EntityId eid, const Transform &t, bool forceFull) const;
  bool CanQuantizeTransformFields(uint8_t fieldMask, const Transform &t) const;
  void RememberSerializedTransform(ecs::EntityId eid, const Transform &t);

  World &world;
  INetworkTransport &net;
  std::unordered_map<ecs::EntityId, NetEntityId> entityToNetId;
  std::unordered_map<ConnectionId, ecs::EntityId> connectionToEntity;
  ecs::EntityId serverPlayerEntityId = ecs::NULL_ENTITY;  // create once; use NULL_ENTITY so id 0 is never "not created"
  static constexpr NetEntityId kServerPlayerNetId = 0;
  NetEntityId nextNetId = 1;
  uint32_t serverTick = 0;
  uint32_t fullSnapshotIntervalTicks = 30;
  bool useQuantizedTransformPayload_ = false;
  struct SerializedTransform {
    float x = 0.f;
    float y = 0.f;
    float rotation = 0.f;
    float scaleX = 1.f;
    float scaleY = 1.f;
  };
  std::unordered_map<ecs::EntityId, SerializedTransform> lastSentTransform_;
  Stats stats_;
};

}  // namespace criogenio
