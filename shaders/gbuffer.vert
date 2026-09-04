#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec3 a_Normal;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_ViewPos;
out vec3 v_ViewNormal;

void main()
{
	vec4 viewPos = u_View * u_Model * vec4(a_Position, 1.0);
	v_ViewPos    = viewPos.xyz;
	v_ViewNormal = normalize(mat3(transpose(inverse(u_View * u_Model))) * a_Normal);
	gl_Position  = u_Projection * viewPos;
}
