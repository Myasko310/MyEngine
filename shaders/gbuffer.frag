#version 330 core

in vec3 v_ViewPos;
in vec3 v_ViewNormal;

layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;

void main()
{
	gPosition = v_ViewPos;
	gNormal   = normalize(v_ViewNormal);
}
