#pragma once
#include "world.h"

namespace criogenio {

bool SaveWorldToFile(const World &world, const std::string &path);
bool LoadWorldFromFile(World &world, const std::string &path);

} // namespace criogenio