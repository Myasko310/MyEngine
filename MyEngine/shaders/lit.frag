#version 330 core

in vec3 v_Position;
in vec3 v_Color;
in vec3 v_Normal;

out vec4 FragColor;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform vec3 u_AmbientColor;

uniform vec3 u_MaterialAlbedo;
uniform float u_MaterialShininess;

void main()
{
	vec3 N = normalize(v_Normal);
	vec3 L = normalize(-u_LightDirection);

	// Ambient
	vec3 ambient = u_AmbientColor * u_MaterialAlbedo;

	// Diffuse
	float diff = max(dot(N, L), 0.0);
	vec3 diffuse = diff * u_LightColor * u_MaterialAlbedo;

	// Specular (Blinn-Phong)
	vec3 V = normalize(-v_Position);
	vec3 H = normalize(L + V);
	float spec = pow(max(dot(N, H), 0.0), u_MaterialShininess);
	vec3 specular = spec * u_LightColor;

	vec3 color = ambient + diffuse + specular;
	color *= v_Color; // modulate by vertex color

	FragColor = vec4(color, 1.0);
}
