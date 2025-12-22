#pragma once

#include "json.hpp"
#include "serialization.h"

namespace criogenio {

using json = nlohmann::json;

json ToJson(const SerializedWorld &world);
SerializedWorld FromJson(const json &j);

json ToJson(const SerializedWorld &world);
SerializedWorld FromJson(const json &json);

} // namespace criogenio