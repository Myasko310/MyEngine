#version 330 core

// Per-vertex: the quad corner offset (in [-0.5, 0.5])
layout(location = 0) in vec2  a_Corner;

// Per-instance data
layout(location = 1) in vec3  a_Position;  // world-space emitter position
layout(location = 2) in float a_Size;      // billboard half-size
layout(location = 3) in vec4  a_Color;     // RGBA
layout(location = 4) in vec4  a_AxisStretch;
layout(location = 5) in vec2  a_MarkAge;

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_CamRight;  // camera right vector (from view matrix column 0)
uniform vec3 u_CamUp;     // camera up vector    (from view matrix column 1)

out vec2  v_TexCoords;
out vec4  v_Color;
flat out vec2 v_MarkAge;

void main()
{
	// Expand the billboard in camera space so it always faces the camera
	vec2 offset = a_Corner;
	if (a_MarkAge.x > 0.5)
	{
		vec2 axis = vec2(dot(a_AxisStretch.xyz,u_CamRight),dot(a_AxisStretch.xyz,u_CamUp));
		float projected = length(axis);
		vec2 along = projected > 0.001 ? axis/projected : vec2(1.0,0.0);
		offset = along * a_Corner.x * mix(1.0,a_AxisStretch.w,min(projected,1.0)) + vec2(-along.y,along.x)*a_Corner.y;
	}
	vec3 worldPos = a_Position + (u_CamRight*offset.x + u_CamUp*offset.y)*a_Size;
	v_MarkAge = a_MarkAge;

	gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);

	// Map corner [-0.5, 0.5] → UV [0, 1]
	v_TexCoords = a_Corner + 0.5;
	v_Color     = a_Color;
}
