#pragma once

#include <cstdint>

namespace criogenio {

enum class MsgType : uint8_t { Input, Snapshot, TerrainDelta };

/** Input state sent from client to server for authoritative simulation. */
struct PlayerInput {
  float move_x = 0.f;
  float move_y = 0.f;
  float aim_x = 0.f;
  float aim_y = 0.f;
  uint8_t action_bits = 0;  // bitmask for fire, jump, etc.
};

/** Component mask bits for snapshot serialization. */
constexpr uint32_t COMPONENT_MASK_TRANSFORM = 1u;

}  // namespace criogenio