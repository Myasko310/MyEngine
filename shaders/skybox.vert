#version 330 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_TexCoords;

void main()
{
	v_TexCoords = a_Position;
	vec4 pos = u_Projection * u_View * vec4(a_Position, 1.0);
	// Force depth to the far plane (w) so the skybox is drawn behind all
	// other geometry regardless of the cube's actual scale.
	gl_Position = pos.xyww;
}
