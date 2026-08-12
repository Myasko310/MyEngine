#version 330 core

in vec2 v_TexCoords;
out vec4 FragColor;

uniform sampler2D u_SceneTexture;
uniform float u_Threshold;

void main()
{
	vec3 color = texture(u_SceneTexture, v_TexCoords).rgb;
	float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
	if (brightness > u_Threshold)
		FragColor = vec4(color, 1.0);
	else
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
