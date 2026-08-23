#version 330 core

in vec3 v_Position;
in vec3 v_Color;
in vec3 v_Normal;
in vec2 v_TexCoords;
in float v_ViewSpaceDepth;
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

// IBL / environment lighting
uniform bool        u_UseIBL;
uniform float       u_IBLIntensity;
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform sampler2D   u_BrdfLUT;

// Cascaded shadow maps
#define MAX_CASCADES 4
uniform bool         u_DirectionalShadowsEnabled;
uniform int          u_NumCascades;
uniform sampler2D    u_CascadeShadowMap[MAX_CASCADES];
uniform mat4         u_CascadeLightSpace[MAX_CASCADES];
uniform float        u_CascadeSplitFar[MAX_CASCADES];
uniform float        u_ShadowBias;

// SSAO
uniform bool      u_SSAOEnabled;
uniform sampler2D u_SSAOTexture;
uniform vec2      u_ScreenSize;

#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4

uniform int u_NumPointLights;
uniform vec3 u_PointLightPos[MAX_POINT_LIGHTS];
uniform vec3 u_PointLightColor[MAX_POINT_LIGHTS];
uniform float u_PointLightRange[MAX_POINT_LIGHTS];
uniform bool u_PointLightCastShadows[MAX_POINT_LIGHTS];
uniform samplerCube u_PointShadowMap[MAX_POINT_LIGHTS];
uniform float u_PointShadowFarPlane[MAX_POINT_LIGHTS];
uniform float u_PointShadowBias[MAX_POINT_LIGHTS];

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

// 3×3 PCF for a single cascade
float ShadowPCF(sampler2D shadowMap, vec4 fragPosLightSpace, float bias, vec2 texelSize)
{
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;
	if (projCoords.z > 1.0)
		return 0.0;
	float currentDepth = projCoords.z;
	float shadow = 0.0;
	for (int x = -1; x <= 1; ++x)
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
		}
	return shadow / 9.0;
}

float ShadowCalculation(vec3 worldPos, vec3 N)
{
	if (!u_DirectionalShadowsEnabled)
		return 0.0;

	int preferredCascade = u_NumCascades - 1;
	for (int i = 0; i < u_NumCascades; ++i)
	{
		if (v_ViewSpaceDepth < u_CascadeSplitFar[i])
		{
			preferredCascade = i;
			break;
		}
	}

	// Fallback to wider cascades when the preferred cascade does not cover
	// this fragment in XY. This reduces camera-distance shadow dropouts.
	for (int cascade = preferredCascade; cascade < u_NumCascades; ++cascade)
	{
		vec4 fragPosLightSpace = u_CascadeLightSpace[cascade] * vec4(worldPos, 1.0);
		vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
		projCoords = projCoords * 0.5 + 0.5;

		if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
			projCoords.y < 0.0 || projCoords.y > 1.0 ||
			projCoords.z < 0.0 || projCoords.z > 1.0)
		{
			continue;
		}

		// Convert user bias from world-ish units into normalized depth for this
		// specific cascade. This keeps bias stable as cascade depth ranges grow.
		float depthScale = max(abs(u_CascadeLightSpace[cascade][2][2]) * 0.5, 0.000001);
		float cascadeBaseBias = u_ShadowBias * depthScale;
		float bias = max(
			cascadeBaseBias * 5.0 * (1.0 - dot(normalize(N), normalize(-u_LightDirection))),
			cascadeBaseBias
		);

		vec2 texelSize = 1.0 / vec2(textureSize(u_CascadeShadowMap[cascade], 0));
		return ShadowPCF(u_CascadeShadowMap[cascade], fragPosLightSpace, bias, texelSize);
	}

	return 0.0;
}

float PointShadowCalculation(int lightIndex, vec3 lightPos, vec3 normal)
{
	if (!u_PointLightCastShadows[lightIndex])
		return 0.0;

	vec3 fragToLight = v_Position - lightPos;
	float currentDepth = length(fragToLight);
	float farPlane = max(u_PointShadowFarPlane[lightIndex], 0.001);
	if (currentDepth >= farPlane)
		return 0.0;

	float closestDepth = texture(u_PointShadowMap[lightIndex], fragToLight).r * farPlane;
	if (closestDepth >= farPlane - 0.001)
		return 0.0;

	vec3 lightDir = normalize(lightPos - v_Position);
	float angularBias = max(0.02 * (1.0 - max(dot(normalize(normal), lightDir), 0.0)), 0.002);
	float bias = max(u_PointShadowBias[lightIndex], angularBias);
	return (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
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

	// Modulate material AO by screen-space AO
	if (u_SSAOEnabled)
	{
		vec2 ssaoUV = gl_FragCoord.xy / u_ScreenSize;
		ao *= texture(u_SSAOTexture, ssaoUV).r;
	}

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

	float shadow = ShadowCalculation(v_Position, N);

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

	vec3 ambient;
	if (u_UseIBL)
	{
		vec3 kS_ibl = FresnelSchlick(max(dot(N, V), 0.0), F0);
		vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);
		vec3 irradiance = texture(u_IrradianceMap, N).rgb;
		vec3 diffuse_ibl = irradiance * albedo;

		vec3 R = reflect(-V, N);
		const float MAX_REFLECTION_LOD = 4.0;
		vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
		vec2 brdf = texture(u_BrdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
		vec3 specular_ibl = prefilteredColor * (kS_ibl * brdf.x + brdf.y);

		ambient = (kD_ibl * diffuse_ibl + specular_ibl) * ao * u_IBLIntensity;
	}
	else
	{
		ambient = u_AmbientColor * albedo * ao;
	}

	vec3 color = ambient + Lo + emissive;

	FragColor = vec4(color, 1.0);
}
