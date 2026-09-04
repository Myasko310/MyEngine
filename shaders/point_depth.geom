#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

uniform mat4 u_ShadowMatrices[6];

out vec3 v_WorldPos;

void main()
{
	for (int face = 0; face < 6; ++face)
	{
		gl_Layer = face;
		for (int i = 0; i < 3; ++i)
		{
			vec4 worldPos = gl_in[i].gl_Position;
			v_WorldPos = worldPos.xyz;
			gl_Position = u_ShadowMatrices[face] * worldPos;
			EmitVertex();
		}
		EndPrimitive();
	}
}
