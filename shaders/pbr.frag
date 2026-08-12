#version 330 core

in vec3 v_Position;
in vec3 v_Color;
in vec3 v_Normal;
in vec2 v_TexCoords;
in vec4 v_FragPosLightSpace;
in mat3 v_TBN;

out vec4 FragColor;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform vec3 u_AmbientColor;
uniform vec3 u_ViewPos;

// Base material factors (used when the corresponding map is not bound)
uniform vec3 u_MaterialAlbedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AOStrength;
uniform vec3 u_Emissive;

// Optional PBR texture maps
uniform bool u_UseAlbedoMap;
uniform sampler2D u_AlbedoMap;

uniform bool u_UseNormalMap;
uniform sampler2D u_NormalMap;

uniform bool u_UseMetallicRoughnessMap;
uniform sampler2D u_MetallicRoughnessMap;

uniform bool u_UseAOMap;
uniform sampler2D u_AOMap;

uniform bool u_UseEmissiveMap;
uniform sampler2D u_EmissiveMap;

uniform sampler2D u_ShadowMap;

#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4

uniform int u_NumPointLights;
uniform vec3 u_PointLightPos[MAX_POINT_LIGHTS];
uniform vec3 u_PointLightColor[MAX_POINT_LIGHTS];
uniform float u_PointLightRange[MAX_POINT_LIGHTS];

uniform int u_NumSpotLights;
uniform vec3 u_SpotLightPos[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightDir[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightColor[MAX_SPOT_LIGHTS];
uniform float u_SpotLightRange[MAX_SPOT_LIGHTS];
uniform float u_SpotLightInnerCos[MAX_SPOT_LIGHTS];
uniform float u_SpotLightOuterCos[MAX_SPOT_LIGHTS];

const float PI = 3.14159265359;

float AttenuationFromRange(float dist, float range)
{
	float rangeClamped = max(range, 0.001);
	float falloff = clamp(1.0 - pow(dist / rangeClamped, 4.0), 0.0, 1.0);
	falloff = falloff * falloff;
	return falloff / (dist * dist + 1.0);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 N)
{
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;
	float closestDepth = texture(u_ShadowMap, projCoords.xy).r;
	float currentDepth = projCoords.z;
	float bias = max(0.005 * (1.0 - dot(N, normalize(-u_LightDirection))), 0.0005);
	float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
	if (projCoords.z > 1.0)
		shadow = 0.0;
	return shadow;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return a2 / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float denom = NdotV * (1.0 - k) + k;
	return NdotV / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
	vec3 albedo = u_UseAlbedoMap ? texture(u_AlbedoMap, v_TexCoords).rgb * u_MaterialAlbedo : u_MaterialAlbedo;

	vec3 N = normalize(v_Normal);
	if (u_UseNormalMap)
	{
		// Sample the tangent-space normal (packed [0,1] -> [-1,1]) and
		// transform it into world space via the TBN basis built in the
		// vertex shader.
		vec3 tangentNormal = texture(u_NormalMap, v_TexCoords).rgb * 2.0 - 1.0;
		N = normalize(v_TBN * tangentNormal);
	}

	float metallic = u_Metallic;
	float roughness = u_Roughness;
	if (u_UseMetallicRoughnessMap)
	{
		vec3 mr = texture(u_MetallicRoughnessMap, v_TexCoords).rgb;
		roughness = clamp(mr.g * u_Roughness, 0.04, 1.0);
		metallic = clamp(mr.b * u_Metallic, 0.0, 1.0);
	}

	float ao = u_UseAOMap ? texture(u_AOMap, v_TexCoords).r * u_AOStrength : u_AOStrength;

	vec3 emissive = u_UseEmissiveMap ? texture(u_EmissiveMap, v_TexCoords).rgb * u_Emissive : u_Emissive;

	vec3 V = normalize(u_ViewPos - v_Position);
	vec3 L = normalize(-u_LightDirection);
	vec3 H = normalize(V + L);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	// Cook-Torrance BRDF (directional light)
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L, roughness);
	vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

	vec3 numerator = NDF * G * F;
	float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
	vec3 specular = numerator / denominator;

	vec3 kS = F;
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

	float NdotL = max(dot(N, L), 0.0);
	vec3 radiance = u_LightColor;

	float shadow = ShadowCalculation(v_FragPosLightSpace, N);

	vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);

	// Point lights
	for (int i = 0; i < u_NumPointLights; ++i)
	{
		vec3 toLight = u_PointLightPos[i] - v_Position;
		float dist = length(toLight);
		vec3 Lp = toLight / max(dist, 0.0001);
		vec3 Hp = normalize(V + Lp);
		float atten = AttenuationFromRange(dist, u_PointLightRange[i]);
		vec3 radianceP = u_PointLightColor[i] * atten;

		float NDFp = DistributionGGX(N, Hp, roughness);
		float Gp = GeometrySmith(N, V, Lp, roughness);
		vec3 Fp = FresnelSchlick(max(dot(Hp, V), 0.0), F0);

		vec3 numeratorP = NDFp * Gp * Fp;
		float denominatorP = 4.0 * max(dot(N, V), 0.0) * max(dot(N, Lp), 0.0) + 0.0001;
		vec3 specularP = numeratorP / denominatorP;

		vec3 kSp = Fp;
		vec3 kDp = (vec3(1.0) - kSp) * (1.0 - metallic);

		float NdotLp = max(dot(N, Lp), 0.0);
		Lo += (kDp * albedo / PI + specularP) * radianceP * NdotLp;
	}

	// Spot lights
	for (int i = 0; i < u_NumSpotLights; ++i)
	{
		vec3 toLight = u_SpotLightPos[i] - v_Position;
		float dist = length(toLight);
		vec3 Ls = toLight / max(dist, 0.0001);
		vec3 Hs = normalize(V + Ls);
		float atten = AttenuationFromRange(dist, u_SpotLightRange[i]);

		float theta = dot(Ls, normalize(-u_SpotLightDir[i]));
		float epsilon = max(u_SpotLightInnerCos[i] - u_SpotLightOuterCos[i], 0.0001);
		float coneFactor = clamp((theta - u_SpotLightOuterCos[i]) / epsilon, 0.0, 1.0);

		vec3 radianceS = u_SpotLightColor[i] * atten * coneFactor;

		float NDFs = DistributionGGX(N, Hs, roughness);
		float Gs = GeometrySmith(N, V, Ls, roughness);
		vec3 Fs = FresnelSchlick(max(dot(Hs, V), 0.0), F0);

		vec3 numeratorS = NDFs * Gs * Fs;
		float denominatorS = 4.0 * max(dot(N, V), 0.0) * max(dot(N, Ls), 0.0) + 0.0001;
		vec3 specularS = numeratorS / denominatorS;

		vec3 kSs = Fs;
		vec3 kDs = (vec3(1.0) - kSs) * (1.0 - metallic);

		float NdotLs = max(dot(N, Ls), 0.0);
		Lo += (kDs * albedo / PI + specularS) * radianceS * NdotLs;
	}

	vec3 ambient = u_AmbientColor * albedo * ao;

	vec3 color = ambient + Lo + emissive;

	FragColor = vec4(color, 1.0);
}
