#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec2 a_TexCoords;
layout(location = 6) in vec3 a_Tangent;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_Position;
out vec3 v_Color;
out vec3 v_Normal;
out vec2 v_TexCoords;
out float v_ViewSpaceDepth;  // positive view-space depth for cascade selection
out mat3 v_TBN;

void main()
{
	v_Position = vec3(u_Model * vec4(a_Position, 1.0));

	mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
	vec3 N = normalize(normalMatrix * a_Normal);
	v_Normal = N;

	// Build the TBN matrix for tangent-space normal mapping.
	vec3 T = a_Tangent;
	if (dot(T, T) > 0.0001)
	{
		T = normalize(normalMatrix * T);
		T = normalize(T - N * dot(N, T));
		vec3 B = cross(N, T);
		v_TBN = mat3(T, B, N);
	}
	else
	{
		v_TBN = mat3(1.0);
	}

	v_Color = a_Color;
	v_TexCoords = a_TexCoords;

	vec4 viewPos = u_View * vec4(v_Position, 1.0);
	v_ViewSpaceDepth = -viewPos.z;
	gl_Position = u_Projection * viewPos;
}
