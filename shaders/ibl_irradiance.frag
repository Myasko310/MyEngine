#version 330 core
in  vec3 v_LocalPos;
out vec4 FragColor;

uniform samplerCube u_EnvironmentMap;

const float PI = 3.14159265359;

void main()
{
	// The irradiance integral: sample the hemisphere around N and
	// average the incoming radiance weighted by cos(theta).
	vec3 N = normalize(v_LocalPos);

	vec3 irradiance = vec3(0.0);
	vec3 up    = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, N));
	up         = normalize(cross(N, right));

	float sampleDelta = 0.025;
	float nrSamples   = 0.0;

	for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
	{
		for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
		{
			// Spherical to Cartesian (tangent space)
			vec3 tangentSample = vec3(sin(theta) * cos(phi),
									 sin(theta) * sin(phi),
									 cos(theta));
			// World space
			vec3 sampleVec = tangentSample.x * right
						   + tangentSample.y * up
						   + tangentSample.z * N;

			irradiance += texture(u_EnvironmentMap, sampleVec).rgb
						* cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	irradiance = PI * irradiance / nrSamples;
	FragColor  = vec4(irradiance, 1.0);
}
