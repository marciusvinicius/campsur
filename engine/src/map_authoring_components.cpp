#include "map_authoring_components.h"

namespace criogenio {

static void putStr(SerializedComponent &o, const char *k, const std::string &v) {
  o.fields[k] = v;
}
static void putBool(SerializedComponent &o, const char *k, bool v) {
  o.fields[k] = v;
}
static void putInt(SerializedComponent &o, const char *k, int v) {
  o.fields[k] = v;
}
static void putF(SerializedComponent &o, const char *k, float v) {
  o.fields[k] = v;
}

SerializedComponent MapEventZone2D::Serialize() const {
  SerializedComponent o;
  o.type = TypeName();
  putStr(o, "storage_key", storage_key);
  putF(o, "width", width);
  putF(o, "height", height);
  putBool(o, "point", point);
  putStr(o, "event_id", event_id);
  putStr(o, "event_trigger", event_trigger);
  putStr(o, "event_type", event_type);
  putStr(o, "object_type", object_type);
  putStr(o, "teleport_to", teleport_to);
  putStr(o, "target_level", target_level);
  putStr(o, "spawn_point", spawn_point);
  putInt(o, "spawn_count", spawn_count);
  putInt(o, "monster_count", monster_count);
  putStr(o, "gameplay_actions", gameplay_actions);
  putBool(o, "activated", activated);
  return o;
}

void MapEventZone2D::Deserialize(const SerializedComponent &data) {
  if (auto it = data.fields.find("storage_key"); it != data.fields.end())
    storage_key = GetString(it->second);
  if (auto it = data.fields.find("width"); it != data.fields.end())
    width = GetFloat(it->second);
  if (auto it = data.fields.find("height"); it != data.fields.end())
    height = GetFloat(it->second);
  if (auto it = data.fields.find("point"); it != data.fields.end())
    point = std::get<bool>(it->second);
  if (auto it = data.fields.find("event_id"); it != data.fields.end())
    event_id = GetString(it->second);
  if (auto it = data.fields.find("event_trigger"); it != data.fields.end())
    event_trigger = GetString(it->second);
  if (auto it = data.fields.find("event_type"); it != data.fields.end())
    event_type = GetString(it->second);
  if (auto it = data.fields.find("object_type"); it != data.fields.end())
    object_type = GetString(it->second);
  if (auto it = data.fields.find("teleport_to"); it != data.fields.end())
    teleport_to = GetString(it->second);
  if (auto it = data.fields.find("target_level"); it != data.fields.end())
    target_level = GetString(it->second);
  if (auto it = data.fields.find("spawn_point"); it != data.fields.end())
    spawn_point = GetString(it->second);
  if (auto it = data.fields.find("spawn_count"); it != data.fields.end())
    spawn_count = GetInt(it->second);
  if (auto it = data.fields.find("monster_count"); it != data.fields.end())
    monster_count = GetInt(it->second);
  if (auto it = data.fields.find("gameplay_actions"); it != data.fields.end())
    gameplay_actions = GetString(it->second);
  if (auto it = data.fields.find("activated"); it != data.fields.end())
    activated = std::get<bool>(it->second);
}

SerializedComponent InteractableZone2D::Serialize() const {
  SerializedComponent o;
  o.type = TypeName();
  putF(o, "width", width);
  putF(o, "height", height);
  putBool(o, "point", point);
  putInt(o, "tiled_object_id", tiled_object_id);
  putStr(o, "interactable_type", interactable_type);
  putStr(o, "properties_json", properties_json);
  return o;
}

void InteractableZone2D::Deserialize(const SerializedComponent &data) {
  if (auto it = data.fields.find("width"); it != data.fields.end())
    width = GetFloat(it->second);
  if (auto it = data.fields.find("height"); it != data.fields.end())
    height = GetFloat(it->second);
  if (auto it = data.fields.find("point"); it != data.fields.end())
    point = std::get<bool>(it->second);
  if (auto it = data.fields.find("tiled_object_id"); it != data.fields.end())
    tiled_object_id = GetInt(it->second);
  if (auto it = data.fields.find("interactable_type"); it != data.fields.end())
    interactable_type = GetString(it->second);
  if (auto it = data.fields.find("properties_json"); it != data.fields.end())
    properties_json = GetString(it->second);
}

SerializedComponent WorldSpawnPrefab2D::Serialize() const {
  SerializedComponent o;
  o.type = TypeName();
  putStr(o, "prefab_name", prefab_name);
  putInt(o, "quantity", quantity);
  putF(o, "width", width);
  putF(o, "height", height);
  return o;
}

void WorldSpawnPrefab2D::Deserialize(const SerializedComponent &data) {
  if (auto it = data.fields.find("prefab_name"); it != data.fields.end())
    prefab_name = GetString(it->second);
  if (auto it = data.fields.find("quantity"); it != data.fields.end())
    quantity = GetInt(it->second);
  if (auto it = data.fields.find("width"); it != data.fields.end())
    width = GetFloat(it->second);
  if (auto it = data.fields.find("height"); it != data.fields.end())
    height = GetFloat(it->second);
}

} // namespace criogenio
