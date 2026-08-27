#pragma once

#include <cstdint>
#include <string>

struct PrefabInstanceComponent
{
	std::string sourcePrefabPath;
	std::uint32_t sourceEntityID = 0;
};
