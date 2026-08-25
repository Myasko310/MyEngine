#include "CollisionMatrix.h"
#include <algorithm>

namespace MyEngine
{
	// All layers enabled by default: every bit set.
	std::array<uint32_t, CollisionMatrix::NUM_LAYERS> CollisionMatrix::s_Rows = []()
	{
		std::array<uint32_t, NUM_LAYERS> rows{};
		rows.fill(0xFFFFFFFFu);
		return rows;
	}();

	bool CollisionMatrix::CanCollide(int layerA, int layerB)
	{
		if (layerA < 0 || layerA >= NUM_LAYERS || layerB < 0 || layerB >= NUM_LAYERS)
			return false;
		return (s_Rows[layerA] & (1u << layerB)) != 0;
	}

	void CollisionMatrix::SetCollision(int layerA, int layerB, bool enabled)
	{
		if (layerA < 0 || layerA >= NUM_LAYERS || layerB < 0 || layerB >= NUM_LAYERS)
			return;
		if (enabled)
		{
			s_Rows[layerA] |=  (1u << layerB);
			s_Rows[layerB] |=  (1u << layerA);
		}
		else
		{
			s_Rows[layerA] &= ~(1u << layerB);
			s_Rows[layerB] &= ~(1u << layerA);
		}
	}

	void CollisionMatrix::Reset()
	{
		s_Rows.fill(0xFFFFFFFFu);
	}

	void CollisionMatrix::SetRow(int layer, uint32_t mask)
	{
		if (layer >= 0 && layer < NUM_LAYERS)
			s_Rows[layer] = mask;
	}
}
