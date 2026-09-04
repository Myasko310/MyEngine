#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace MyEngine
{
	// Maximum number of layers supported.
	constexpr int MAX_LAYERS = 32;

	// Global registry mapping layer indices [0-31] to human-readable names.
	// Layer 0 is always "Default".
	class LayerMask
	{
	public:
		// Get/set the name of a layer by index.
		static const std::string& GetName(int index);
		static void               SetName(int index, const std::string& name);

		// Return a bitmask with bit `index` set.
		static uint32_t MaskOf(int index) { return (index >= 0 && index < MAX_LAYERS) ? (1u << index) : 0u; }

		// Return whether layer `index` is set in `mask`.
		static bool InMask(int index, uint32_t mask) { return (MaskOf(index) & mask) != 0; }

		// Access the raw name array (useful for editor UI iteration).
		static const std::array<std::string, MAX_LAYERS>& GetNames() { return s_Names; }

		// Reset all names to defaults.
		static void Reset();

	private:
		static std::array<std::string, MAX_LAYERS> s_Names;
	};
}
