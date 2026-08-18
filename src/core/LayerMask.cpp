#include "core/LayerMask.h"

namespace MyEngine
{
	// Default layer names – only the first few are pre-named;
	// the rest start as "Layer N" and can be renamed by the user.
	std::array<std::string, MAX_LAYERS> LayerMask::s_Names = []()
	{
		std::array<std::string, MAX_LAYERS> names;
		names[0]  = "Default";
		names[1]  = "TransparentFX";
		names[2]  = "IgnoreRaycast";
		names[3]  = "Water";
		names[4]  = "UI";
		for (int i = 5; i < MAX_LAYERS; ++i)
			names[i] = "Layer " + std::to_string(i);
		return names;
	}();

	const std::string& LayerMask::GetName(int index)
	{
		static const std::string empty;
		if (index < 0 || index >= MAX_LAYERS) return empty;
		return s_Names[index];
	}

	void LayerMask::SetName(int index, const std::string& name)
	{
		if (index >= 0 && index < MAX_LAYERS)
			s_Names[index] = name;
	}

	void LayerMask::Reset()
	{
		s_Names[0] = "Default";
		s_Names[1] = "TransparentFX";
		s_Names[2] = "IgnoreRaycast";
		s_Names[3] = "Water";
		s_Names[4] = "UI";
		for (int i = 5; i < MAX_LAYERS; ++i)
			s_Names[i] = "Layer " + std::to_string(i);
	}
}
