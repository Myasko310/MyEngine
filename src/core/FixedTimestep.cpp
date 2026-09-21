#include "core/FixedTimestep.h"
#include <algorithm>

namespace MyEngine
{
	FixedTimestep::FixedTimestep(float fixedDeltaTime)
		: m_FixedDeltaTime(fixedDeltaTime)
		, m_Accumulator(0.0f)
		, m_InterpolationFactor(0.0f)
		, m_MaxSubsteps(5)
		, m_SubstepsThisFrame(0)
	{
	}

	void FixedTimestep::Update(float deltaTime)
	{
		// Clamp deltaTime to prevent spiral of death
		deltaTime = std::min(deltaTime, 0.1f);

		m_Accumulator += deltaTime;
		m_SubstepsThisFrame = 0;
	}

	bool FixedTimestep::ShouldFixedUpdate()
	{
		if (m_Accumulator >= m_FixedDeltaTime && m_SubstepsThisFrame < m_MaxSubsteps)
		{
			m_Accumulator -= m_FixedDeltaTime;
			m_SubstepsThisFrame++;

			// Calculate interpolation factor for rendering
			// This represents how far we are into the next physics frame
			m_InterpolationFactor = m_Accumulator / m_FixedDeltaTime;

			return true;
		}

		return false;
	}

	void FixedTimestep::Reset()
	{
		m_Accumulator = 0.0f;
		m_InterpolationFactor = 0.0f;
		m_SubstepsThisFrame = 0;
	}
}
