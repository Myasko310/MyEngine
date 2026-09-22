#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec2 a_TexCoords;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_Position;
out vec3 v_Color;
out vec3 v_Normal;
out vec2 v_TexCoords;
out float v_ViewSpaceDepth;

void main()
{
	v_Position = vec3(u_Model * vec4(a_Position, 1.0));
	v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
	v_Color = vec3(1.0);

	// Arena box faces repeat every four world units instead of stretching one tile
	// over an entire wall. Object-relative coordinates keep moving surfaces stable.
	vec3 scaledPosition = a_Position * vec3(length(u_Model[0].xyz), length(u_Model[1].xyz), length(u_Model[2].xyz));
	vec3 faceNormal = abs(a_Normal);
	if (faceNormal.y >= faceNormal.x && faceNormal.y >= faceNormal.z)
		v_TexCoords = scaledPosition.xz * 0.25;
	else if (faceNormal.x >= faceNormal.z)
		v_TexCoords = scaledPosition.zy * 0.25;
	else
		v_TexCoords = scaledPosition.xy * 0.25;

	vec4 viewPos = u_View * vec4(v_Position, 1.0);
	v_ViewSpaceDepth = -viewPos.z;
	gl_Position = u_Projection * viewPos;
}
