#version 330 core

in vec3 v_WorldPos;

uniform vec3 u_LightPos;
uniform float u_FarPlane;

void main()
{
	float lightDistance = length(v_WorldPos - u_LightPos);
	lightDistance = clamp(lightDistance / max(u_FarPlane, 0.001), 0.0, 1.0);
	gl_FragDepth = lightDistance;
}
