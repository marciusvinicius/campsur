#pragma once

#include "network/net_entity.h"
#include <cstdint>

namespace criogenio {
struct SnapshotEntity {
  NetEntityId id;
  uint32_t componentMask;
  std::vector<uint8_t> data;
};

struct Snapshot {
  uint32_t tick;
  std::vector<SnapshotEntity> entities;
};
} // namespace criogenio