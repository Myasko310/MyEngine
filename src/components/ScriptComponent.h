#pragma once

#include <string>

struct ScriptComponent
{
	std::string scriptPath;
	bool enabled = true;
	bool autoStart = true;
	bool requestReload = false;
};
