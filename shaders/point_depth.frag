#version 330 core

in vec3 v_WorldPos;

uniform vec3 u_LightPos;
uniform float u_FarPlane;

void main()
{
	float lightDistance = length(v_WorldPos - u_LightPos);
	lightDistance = lightDistance / u_FarPlane;
	gl_FragDepth = lightDistance;
}
