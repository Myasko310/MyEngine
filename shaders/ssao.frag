#version 330 core

in vec2 v_TexCoords;
out float FragOcclusion;

uniform sampler2D u_Position;   // view-space positions (RGB16F)
uniform sampler2D u_Normal;     // view-space normals   (RGB16F)
uniform sampler2D u_Noise;      // 4×4 random rotation  (RGB16F, tiled)

#define KERNEL_SIZE 64
uniform vec3  u_Samples[KERNEL_SIZE];
uniform mat4  u_Projection;
uniform mat4  u_View;
uniform float u_Radius;
uniform float u_Bias;
uniform float u_Power;
uniform vec2  u_NoiseScale;     // screenSize / 4

void main()
{
	vec3 fragPos = texture(u_Position, v_TexCoords).xyz;
	vec3 normal  = normalize(texture(u_Normal, v_TexCoords).xyz);

	// If no geometry was written at this pixel (background), no occlusion
	if (dot(fragPos, fragPos) < 0.0001)
	{
		FragOcclusion = 1.0;
		return;
	}

	// Build TBN from the noise texture's random rotation vector
	vec3 randomVec = normalize(texture(u_Noise, v_TexCoords * u_NoiseScale).xyz);
	vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN       = mat3(tangent, bitangent, normal);

	float occlusion = 0.0;
	for (int i = 0; i < KERNEL_SIZE; ++i)
	{
		// Sample position in view space
		vec3 samplePos = TBN * u_Samples[i];
		samplePos = fragPos + samplePos * u_Radius;

		// Project sample to get its texture coordinates
		vec4 offset = u_Projection * vec4(samplePos, 1.0);
		offset.xyz /= offset.w;
		offset.xyz  = offset.xyz * 0.5 + 0.5;

		// Get the geometry depth at the sample position
		float sampleDepth = texture(u_Position, offset.xy).z;

		// Range check + accumulate
		float rangeCheck = smoothstep(0.0, 1.0, u_Radius / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + u_Bias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));
	FragOcclusion = pow(occlusion, u_Power);
}
