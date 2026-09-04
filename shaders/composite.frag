#version 330 core

in vec2 v_TexCoords;
out vec4 FragColor;

uniform sampler2D u_SceneTexture;
uniform sampler2D u_BloomTexture;
uniform bool u_BloomEnabled;
uniform float u_Exposure;
uniform float u_BloomIntensity;

void main()
{
	vec3 color = texture(u_SceneTexture, v_TexCoords).rgb;

	if (u_BloomEnabled)
	{
		vec3 bloom = texture(u_BloomTexture, v_TexCoords).rgb;
		color += bloom * u_BloomIntensity;
	}

	// Exposure tone mapping (Reinhard)
	vec3 mapped = vec3(1.0) - exp(-color * u_Exposure);

	// Gamma correction
	mapped = pow(mapped, vec3(1.0 / 2.2));

	FragColor = vec4(mapped, 1.0);
}
