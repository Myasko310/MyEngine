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
uniform vec3 u_ViewPos;

uniform vec3 u_MaterialAlbedo;
uniform float u_MaterialShininess;
uniform bool u_UseTexture;
uniform sampler2D u_Texture;
uniform sampler2D u_ShadowMap;
uniform bool u_DirectionalShadowsEnabled;

#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4

uniform int u_NumPointLights;
uniform vec3 u_PointLightPos[MAX_POINT_LIGHTS];
uniform vec3 u_PointLightColor[MAX_POINT_LIGHTS];
uniform float u_PointLightRange[MAX_POINT_LIGHTS];
uniform bool u_PointLightCastShadows[MAX_POINT_LIGHTS];
uniform samplerCube u_PointShadowMap[MAX_POINT_LIGHTS];
uniform float u_PointShadowFarPlane[MAX_POINT_LIGHTS];
uniform float u_PointShadowBias;

uniform int u_NumSpotLights;
uniform vec3 u_SpotLightPos[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightDir[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightColor[MAX_SPOT_LIGHTS];
uniform float u_SpotLightRange[MAX_SPOT_LIGHTS];
uniform float u_SpotLightInnerCos[MAX_SPOT_LIGHTS];
uniform float u_SpotLightOuterCos[MAX_SPOT_LIGHTS];

float AttenuationFromRange(float dist, float range)
{
	float rangeClamped = max(range, 0.001);
	float falloff = clamp(1.0 - pow(dist / rangeClamped, 4.0), 0.0, 1.0);
	falloff = falloff * falloff;
	return falloff / (dist * dist + 1.0);
}

float ShadowCalculation(vec4 fragPosLightSpace)
{
	if (!u_DirectionalShadowsEnabled)
		return 0.0;

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

float PointShadowCalculation(int lightIndex, vec3 lightPos)
{
	if (!u_PointLightCastShadows[lightIndex])
		return 0.0;

	vec3 fragToLight = v_Position - lightPos;
	float currentDepth = length(fragToLight);
	float farPlane = max(u_PointShadowFarPlane[lightIndex], 0.001);
	float closestDepth = texture(u_PointShadowMap[lightIndex], fragToLight).r * farPlane;

	float bias = max(u_PointShadowBias, 0.0001);
	return (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
}

void main()
{
	vec3 N = normalize(v_Normal);
	vec3 V = normalize(u_ViewPos - v_Position);

	// Sample texture if enabled, otherwise use material albedo
	vec3 baseColor = u_UseTexture ? texture(u_Texture, v_TexCoords).rgb : u_MaterialAlbedo;

	// Ambient
	vec3 ambient = u_AmbientColor * baseColor;

	// Directional light (diffuse + specular)
	vec3 L = normalize(-u_LightDirection);
	float diff = max(dot(N, L), 0.0);
	vec3 diffuse = diff * u_LightColor * baseColor;
	vec3 H = normalize(L + V);
	float spec = pow(max(dot(N, H), 0.0), u_MaterialShininess);
	vec3 specular = spec * u_LightColor;

	float shadow = ShadowCalculation(v_FragPosLightSpace);
	vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specular);

	// Point lights
	for (int i = 0; i < u_NumPointLights; ++i)
	{
		vec3 toLight = u_PointLightPos[i] - v_Position;
		float dist = length(toLight);
		vec3 Lp = toLight / max(dist, 0.0001);
		float atten = AttenuationFromRange(dist, u_PointLightRange[i]);

		float diffP = max(dot(N, Lp), 0.0);
		vec3 diffuseP = diffP * u_PointLightColor[i] * baseColor;

		vec3 Hp = normalize(Lp + V);
		float specP = pow(max(dot(N, Hp), 0.0), u_MaterialShininess);
		vec3 specularP = specP * u_PointLightColor[i];

		float pointShadow = PointShadowCalculation(i, u_PointLightPos[i]);
		lighting += (1.0 - pointShadow) * (diffuseP + specularP) * atten;
	}

	// Spot lights
	for (int i = 0; i < u_NumSpotLights; ++i)
	{
		vec3 toLight = u_SpotLightPos[i] - v_Position;
		float dist = length(toLight);
		vec3 Ls = toLight / max(dist, 0.0001);
		float atten = AttenuationFromRange(dist, u_SpotLightRange[i]);

		float theta = dot(Ls, normalize(-u_SpotLightDir[i]));
		float epsilon = max(u_SpotLightInnerCos[i] - u_SpotLightOuterCos[i], 0.0001);
		float coneFactor = clamp((theta - u_SpotLightOuterCos[i]) / epsilon, 0.0, 1.0);

		float diffS = max(dot(N, Ls), 0.0);
		vec3 diffuseS = diffS * u_SpotLightColor[i] * baseColor;

		vec3 Hs = normalize(Ls + V);
		float specS = pow(max(dot(N, Hs), 0.0), u_MaterialShininess);
		vec3 specularS = specS * u_SpotLightColor[i];

		lighting += (diffuseS + specularS) * atten * coneFactor;
	}

	// Only multiply by vertex color if NOT using texture
	vec3 color = u_UseTexture ? lighting : lighting * v_Color;

	FragColor = vec4(color, 1.0);
}
