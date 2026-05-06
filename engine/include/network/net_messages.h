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

/** Subfield mask bits for transform payload delta packing. */
constexpr uint8_t TRANSFORM_FIELD_X = 1u << 0;
constexpr uint8_t TRANSFORM_FIELD_Y = 1u << 1;
constexpr uint8_t TRANSFORM_FIELD_ROTATION = 1u << 2;
constexpr uint8_t TRANSFORM_FIELD_SCALE_X = 1u << 3;
constexpr uint8_t TRANSFORM_FIELD_SCALE_Y = 1u << 4;
constexpr uint8_t TRANSFORM_FIELDS_ALL = TRANSFORM_FIELD_X | TRANSFORM_FIELD_Y |
                                         TRANSFORM_FIELD_ROTATION | TRANSFORM_FIELD_SCALE_X |
                                         TRANSFORM_FIELD_SCALE_Y;
/** Header flag bit indicating transform values are quantized int16 payloads. */
constexpr uint8_t TRANSFORM_PAYLOAD_QUANTIZED = 1u << 7;

/** Shared quantization scales for optional compact transform payloads. */
constexpr float TRANSFORM_QUANT_POS_SCALE = 64.f;
constexpr float TRANSFORM_QUANT_ROTATION_SCALE = 100.f;
constexpr float TRANSFORM_QUANT_SCALE_SCALE = 1000.f;

}  // namespace criogenio