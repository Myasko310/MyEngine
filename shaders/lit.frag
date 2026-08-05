#version 330 core

in vec3 v_Position;
in vec3 v_Color;
in vec3 v_Normal;
in vec2 v_TexCoords;
in vec4 v_FragPosLightSpace;

out vec4 FragColor;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform vec3 u_AmbientColor;

uniform vec3 u_MaterialAlbedo;
uniform float u_MaterialShininess;
uniform bool u_UseTexture;
uniform sampler2D u_Texture;
uniform sampler2D u_ShadowMap;

float ShadowCalculation(vec4 fragPosLightSpace)
{
	// perform perspective divide
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	// transform to [0,1]
	projCoords = projCoords * 0.5 + 0.5;
	// get closest depth value from light's perspective (using [0,1] range)
	float closestDepth = texture(u_ShadowMap, projCoords.xy).r;
	// current depth
	float currentDepth = projCoords.z;
	// bias to avoid shadow acne
	float bias = max(0.005 * (1.0 - dot(normalize(v_Normal), normalize(-u_LightDirection))), 0.0005);
	// simple shadow
	float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
	// if outside far plane
	if (projCoords.z > 1.0)
		shadow = 0.0;
	return shadow;
}

void main()
{
	vec3 N = normalize(v_Normal);
	vec3 L = normalize(-u_LightDirection);

	// Sample texture if enabled, otherwise use material albedo
	vec3 baseColor = u_UseTexture ? texture(u_Texture, v_TexCoords).rgb : u_MaterialAlbedo;

	// Ambient
	vec3 ambient = u_AmbientColor * baseColor;

	// Diffuse
	float diff = max(dot(N, L), 0.0);
	vec3 diffuse = diff * u_LightColor * baseColor;

	// Specular (Blinn-Phong)
	vec3 V = normalize(-v_Position);
	vec3 H = normalize(L + V);
	float spec = pow(max(dot(N, H), 0.0), u_MaterialShininess);
	vec3 specular = spec * u_LightColor;

	float shadow = ShadowCalculation(v_FragPosLightSpace);
	vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specular);

	// Only multiply by vertex color if NOT using texture
	vec3 color = u_UseTexture ? lighting : lighting * v_Color;

	FragColor = vec4(color, 1.0);
}
