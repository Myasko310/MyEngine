#include "renderer/FrustumCuller.h"

#include <glm/gtc/matrix_access.hpp>
#include <cmath>

namespace MyEngine
{
	void FrustumCuller::Update(const glm::mat4& vp)
	{
		// Extract planes from view-projection matrix (column-major)
		// Left
		m_Planes[0] = glm::vec4(
			vp[0][3] + vp[0][0],
			vp[1][3] + vp[1][0],
			vp[2][3] + vp[2][0],
			vp[3][3] + vp[3][0]
		);

		// Right
		m_Planes[1] = glm::vec4(
			vp[0][3] - vp[0][0],
			vp[1][3] - vp[1][0],
			vp[2][3] - vp[2][0],
			vp[3][3] - vp[3][0]
		);

		// Bottom
		m_Planes[2] = glm::vec4(
			vp[0][3] + vp[0][1],
			vp[1][3] + vp[1][1],
			vp[2][3] + vp[2][1],
			vp[3][3] + vp[3][1]
		);

		// Top
		m_Planes[3] = glm::vec4(
			vp[0][3] - vp[0][1],
			vp[1][3] - vp[1][1],
			vp[2][3] - vp[2][1],
			vp[3][3] - vp[3][1]
		);

		// Near
		m_Planes[4] = glm::vec4(
			vp[0][3] + vp[0][2],
			vp[1][3] + vp[1][2],
			vp[2][3] + vp[2][2],
			vp[3][3] + vp[3][2]
		);

		// Far
		m_Planes[5] = glm::vec4(
			vp[0][3] - vp[0][2],
			vp[1][3] - vp[1][2],
			vp[2][3] - vp[2][2],
			vp[3][3] - vp[3][2]
		);

		// Normalize planes
		for (int i = 0; i < 6; ++i)
		{
			float length = std::sqrt(
				m_Planes[i].x * m_Planes[i].x +
				m_Planes[i].y * m_Planes[i].y +
				m_Planes[i].z * m_Planes[i].z
			);

			if (length > 0.0f)
				m_Planes[i] /= length;
		}
	}

	bool FrustumCuller::IsSphereVisible(const glm::vec3& center, float radius) const
	{
		for (int i = 0; i < 6; ++i)
		{
			const glm::vec4& p = m_Planes[i];
			float distance = p.x * center.x + p.y * center.y + p.z * center.z + p.w;
			if (distance < -radius)
				return false;
		}
		return true;
	}
}
