#include "criogenio_io.h"
#include "json_serialization.h"
#include <fstream>

namespace criogenio {

bool SaveWorldToFile(const World &world, const std::string &path) {
  SerializedWorld sw = world.Serialize();
  json j = ToJson(sw);

  std::ofstream file(path);
  if (!file.is_open())
    return false;

  file << j.dump(2); // pretty-print
  return true;
}

bool LoadWorldFromFile(World &world, const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return false;

  json j;
  file >> j;

  SerializedWorld sw = FromJson(j);
  world.Deserialize(sw);
  return true;
}

bool SaveWorldToFile(const World2 &world, const std::string &path) {
  SerializedWorld sw = world.Serialize();
  json j = ToJson(sw);

  std::ofstream file(path);
  if (!file.is_open())
    return false;

  file << j.dump(2);
  return true;
}

bool LoadWorldFromFile(World2 &world, const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return false;

  json j;
  file >> j;

  SerializedWorld sw = FromJson(j);
  world.Deserialize(sw);
  return true;
}

} // namespace criogenio