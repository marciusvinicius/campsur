#pragma once

#include "network/serialization.h"

namespace criogenio {
struct INetSerializable {
  virtual void Serialize(NetWriter &) const = 0;
  virtual void Deserialize(NetReader &) = 0;
};
} // namespace criogenio
