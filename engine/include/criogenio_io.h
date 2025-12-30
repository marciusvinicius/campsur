#pragma once
#include "world.h"
#include "world2.h"

namespace criogenio {

bool SaveWorldToFile(const World &world, const std::string &path);
bool LoadWorldFromFile(World &world, const std::string &path);

// Overloads for new World2
bool SaveWorldToFile(const World2 &world, const std::string &path);
bool LoadWorldFromFile(World2 &world, const std::string &path);

} // namespace criogenio