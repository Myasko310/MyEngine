#pragma once

#include <array>
#include <cstdint>

namespace MyEngine
{
	// 32x32 symmetric matrix controlling which physics layers can collide with
	// which other layers.  Bit j of row[i] == 1 means layer i collides with
	// layer j.  The matrix is kept symmetric: setting (i,j) also sets (j,i).
	//
	// Default state: every layer collides with every other layer.
	class CollisionMatrix
	{
	public:
		static constexpr int NUM_LAYERS = 32;

		// Returns true if the two layers are allowed to collide.
		static bool CanCollide(int layerA, int layerB);

		// Enable or disable collision between two layers (symmetric).
		static void SetCollision(int layerA, int layerB, bool enabled);

		// Reset all pairs to enabled (default state).
		static void Reset();

		// Raw read/write access for serialization.
		static const std::array<uint32_t, NUM_LAYERS>& GetRows() { return s_Rows; }
		static void SetRow(int layer, uint32_t mask);

	private:
		// s_Rows[i] is a bitmask: bit j is set when layer i collides with layer j.
		static std::array<uint32_t, NUM_LAYERS> s_Rows;
	};
}
