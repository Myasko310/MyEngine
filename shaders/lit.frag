#version 330 core

in vec3 v_Position;
in vec3 v_Color;
in vec3 v_Normal;
in vec2 v_TexCoords;
in float v_ViewSpaceDepth;

out vec4 FragColor;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform vec3 u_AmbientColor;
uniform vec3 u_ViewPos;

uniform vec3 u_MaterialAlbedo;
uniform float u_MaterialShininess;
uniform bool u_UseTexture;
uniform sampler2D u_Texture;
uniform bool u_DirectionalShadowsEnabled;

// Cascaded shadow maps
#define MAX_CASCADES 4
uniform int          u_NumCascades;
uniform sampler2D    u_CascadeShadowMap[MAX_CASCADES];
uniform mat4         u_CascadeLightSpace[MAX_CASCADES];
uniform float        u_CascadeSplitFar[MAX_CASCADES];
uniform float        u_CascadeBlendStart[MAX_CASCADES];
uniform float        u_CascadeBlendFactor;
uniform float        u_ShadowBias;

// SSAO
uniform bool      u_SSAOEnabled;
uniform sampler2D u_SSAOTexture;
uniform vec2      u_ScreenSize;
uniform int       u_DebugViewMode;

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
uniform int u_PointShadowPCFSamples;
uniform float u_PointShadowPCFRadius;
uniform int u_PointShadowPCFSamplesPerLight[MAX_POINT_LIGHTS];
uniform float u_PointShadowPCFRadiusPerLight[MAX_POINT_LIGHTS];

uniform int u_NumSpotLights;
uniform vec3 u_SpotLightPos[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightDir[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightColor[MAX_SPOT_LIGHTS];
uniform float u_SpotLightRange[MAX_SPOT_LIGHTS];
uniform float u_SpotLightInnerCos[MAX_SPOT_LIGHTS];
uniform float u_SpotLightOuterCos[MAX_SPOT_LIGHTS];
uniform bool u_SpotLightCastShadows[MAX_SPOT_LIGHTS];
uniform mat4 u_SpotLightSpace[MAX_SPOT_LIGHTS];
uniform sampler2D u_SpotShadowMap[MAX_SPOT_LIGHTS];
uniform float u_SpotShadowBias[MAX_SPOT_LIGHTS];
uniform float u_SpotShadowPCFRadius;
uniform float u_SpotShadowPCFRadiusPerLight[MAX_SPOT_LIGHTS];

float AttenuationFromRange(float dist, float range)
{
	float rangeClamped = max(range, 0.001);
	float falloff = clamp(1.0 - pow(dist / rangeClamped, 4.0), 0.0, 1.0);
	falloff = falloff * falloff;
	return falloff / (dist * dist + 1.0);
}

// 3x3 PCF for a single cascade
float ShadowPCF(sampler2D shadowMap, vec4 fragPosLightSpace, float bias, vec2 texelSize)
{
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;
	if (projCoords.z > 1.0)
		return 0.0;
	float currentDepth = projCoords.z;
	float shadow = 0.0;
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
		}
	}
	return shadow / 9.0;
}

float ShadowCalculation(vec3 worldPos, vec3 normal)
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

	// Fallback strategy: if the depth-selected cascade does not cover this
	// fragment in XY, try wider cascades before giving up. This prevents
	// distance-dependent shadow dropouts when moving the camera.
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
			cascadeBaseBias * 5.0 * (1.0 - dot(normalize(normal), normalize(-u_LightDirection))),
			cascadeBaseBias
		);

		vec2 texelSize = 1.0 / vec2(textureSize(u_CascadeShadowMap[cascade], 0));
		float baseShadow = ShadowPCF(u_CascadeShadowMap[cascade], fragPosLightSpace, bias, texelSize);

		float blendWidthFactor = clamp(u_CascadeBlendFactor, 0.0, 0.45);
		if (cascade + 1 < u_NumCascades && blendWidthFactor > 0.0001)
		{
			float blendStart = u_CascadeBlendStart[cascade];
			float blendEnd = u_CascadeSplitFar[cascade];
			if (v_ViewSpaceDepth >= blendStart && v_ViewSpaceDepth < blendEnd)
			{
				vec4 nextFragPosLightSpace = u_CascadeLightSpace[cascade + 1] * vec4(worldPos, 1.0);
				vec3 nextProj = nextFragPosLightSpace.xyz / nextFragPosLightSpace.w;
				nextProj = nextProj * 0.5 + 0.5;
				if (nextProj.x >= 0.0 && nextProj.x <= 1.0 && nextProj.y >= 0.0 && nextProj.y <= 1.0 && nextProj.z >= 0.0 && nextProj.z <= 1.0)
				{
					float nextDepthScale = max(abs(u_CascadeLightSpace[cascade + 1][2][2]) * 0.5, 0.000001);
					float nextBaseBias = u_ShadowBias * nextDepthScale;
					float nextBias = max(
						nextBaseBias * 5.0 * (1.0 - dot(normalize(normal), normalize(-u_LightDirection))),
						nextBaseBias
					);
					vec2 nextTexelSize = 1.0 / vec2(textureSize(u_CascadeShadowMap[cascade + 1], 0));
					float nextShadow = ShadowPCF(u_CascadeShadowMap[cascade + 1], nextFragPosLightSpace, nextBias, nextTexelSize);
					float t = clamp((v_ViewSpaceDepth - blendStart) / max(blendEnd - blendStart, 0.0001), 0.0, 1.0);
					return mix(baseShadow, nextShadow, t);
				}
			}
		}
		return baseShadow;
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

	vec3 lightDir = normalize(lightPos - v_Position);
	float angularBias = max(0.02 * (1.0 - max(dot(normalize(normal), lightDir), 0.0)), 0.002);
	float bias = max(u_PointShadowBias[lightIndex], angularBias);

	// PCF: sample the cubemap in a small disk of offset directions. The disk
	// radius scales with distance so penumbra stays visually consistent.
	const vec3 sampleOffsetDirections[20] = vec3[](
		vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
		vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
		vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
		vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
		vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
	);

	int sampleCount = clamp(u_PointShadowPCFSamplesPerLight[lightIndex] > 0 ? u_PointShadowPCFSamplesPerLight[lightIndex] : u_PointShadowPCFSamples, 1, 20);
	float baseRadius = u_PointShadowPCFRadiusPerLight[lightIndex] > 0.0 ? u_PointShadowPCFRadiusPerLight[lightIndex] : u_PointShadowPCFRadius;
	float diskRadius = (1.0 + currentDepth / farPlane) * max(baseRadius, 0.0001);
	float shadow = 0.0;
	for (int s = 0; s < sampleCount; ++s)
	{
		float closestDepth = texture(u_PointShadowMap[lightIndex], fragToLight + sampleOffsetDirections[s] * diskRadius).r * farPlane;
		if (closestDepth < farPlane - 0.001 && currentDepth - bias > closestDepth)
			shadow += 1.0;
	}
	return shadow / float(sampleCount);
}

float SpotShadowCalculation(int lightIndex, vec3 normal)
{
	if (!u_SpotLightCastShadows[lightIndex])
		return 0.0;

	vec4 fragPosLightSpace = u_SpotLightSpace[lightIndex] * vec4(v_Position, 1.0);
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
		projCoords.y < 0.0 || projCoords.y > 1.0 ||
		projCoords.z < 0.0 || projCoords.z > 1.0)
	{
		return 0.0;
	}

	vec3 lightDir = normalize(u_SpotLightPos[lightIndex] - v_Position);
	float bias = max(u_SpotShadowBias[lightIndex] * 5.0 * (1.0 - max(dot(normalize(normal), lightDir), 0.0)), u_SpotShadowBias[lightIndex]);

	// 3x3 PCF
	float spotRadius = u_SpotShadowPCFRadiusPerLight[lightIndex] > 0.0 ? u_SpotShadowPCFRadiusPerLight[lightIndex] : u_SpotShadowPCFRadius;
	vec2 texelSize = (1.0 / vec2(textureSize(u_SpotShadowMap[lightIndex], 0))) * max(spotRadius, 0.1);
	float currentDepth = projCoords.z;
	float shadow = 0.0;
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(u_SpotShadowMap[lightIndex], projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
		}
	}
	return shadow / 9.0;
}

void main()
{
	vec3 N = normalize(v_Normal);
	vec3 V = normalize(u_ViewPos - v_Position);

	vec3 baseColor = u_UseTexture ? texture(u_Texture, v_TexCoords).rgb : u_MaterialAlbedo;

	// Ambient — modulated by SSAO if enabled
	float ssao = 1.0;
	if (u_SSAOEnabled)
	{
		vec2 ssaoUV = gl_FragCoord.xy / u_ScreenSize;
		ssao = texture(u_SSAOTexture, ssaoUV).r;
	}
	vec3 ambient = u_AmbientColor * baseColor * ssao;

	// Directional light
	vec3 L = normalize(-u_LightDirection);
	float diff = max(dot(N, L), 0.0);
	vec3 diffuse = diff * u_LightColor * baseColor;
	vec3 H = normalize(L + V);
	float spec = pow(max(dot(N, H), 0.0), u_MaterialShininess);
	vec3 specular = spec * u_LightColor;

	float shadow = ShadowCalculation(v_Position, N);
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

		float pointShadow = PointShadowCalculation(i, u_PointLightPos[i], N);
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

		float spotShadow = SpotShadowCalculation(i, N);
		lighting += (1.0 - spotShadow) * (diffuseS + specularS) * atten * coneFactor;
	}

	vec3 color = u_UseTexture ? lighting : lighting * v_Color;

	if (u_DebugViewMode == 1)
	{
		FragColor = vec4(baseColor, 1.0);
		return;
	}
	if (u_DebugViewMode == 2)
	{
		FragColor = vec4(normalize(N) * 0.5 + 0.5, 1.0);
		return;
	}
	if (u_DebugViewMode == 3)
	{
		float roughnessDbg = sqrt(clamp(2.0 / max(u_MaterialShininess, 2.0), 0.0, 1.0));
		FragColor = vec4(vec3(roughnessDbg), 1.0);
		return;
	}
	if (u_DebugViewMode == 4)
	{
		FragColor = vec4(vec3(0.0), 1.0);
		return;
	}
	if (u_DebugViewMode == 5)
	{
		FragColor = vec4(vec3(ssao), 1.0);
		return;
	}
	if (u_DebugViewMode == 6)
	{
		FragColor = vec4(vec3(0.0), 1.0);
		return;
	}
	if (u_DebugViewMode == 7)
	{
		FragColor = vec4(vec3(1.0 - shadow), 1.0);
		return;
	}
	if (u_DebugViewMode == 8)
	{
		FragColor = vec4(vec3(ssao), 1.0);
		return;
	}

	FragColor = vec4(color, 1.0);
}
