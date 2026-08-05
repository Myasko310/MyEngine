#pragma once

#include <glm/glm.hpp>

namespace MyEngine
{
	class FrustumCuller
	{
	public:
		// Build frustum planes from view-projection matrix
		void Update(const glm::mat4& vp);

		// Test sphere against frustum. Returns true if inside or intersects.
		bool IsSphereVisible(const glm::vec3& center, float radius) const;

	private:
		// planes: each as (a,b,c,d) where ax+by+cz+d >= 0 is inside
		glm::vec4 m_Planes[6];
	};
}
