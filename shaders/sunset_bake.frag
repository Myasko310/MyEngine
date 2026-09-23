#version 330 core

out vec4 FragColor;
uniform int u_Face;
uniform float u_Resolution;

float hash(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

float noise(vec3 p)
{
	vec3 cell = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(cell), hash(cell + vec3(1,0,0)), f.x),
		mix(hash(cell + vec3(0,1,0)), hash(cell + vec3(1,1,0)), f.x), f.y),
		mix(mix(hash(cell + vec3(0,0,1)), hash(cell + vec3(1,0,1)), f.x),
		mix(hash(cell + vec3(0,1,1)), hash(cell + vec3(1,1,1)), f.x), f.y), f.z);
}

float fbm(vec3 p)
{
	float result = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 6; ++i)
	{
		result += amplitude * noise(p);
		p = p * 2.03 + vec3(17.1, 9.2, 13.7);
		amplitude *= 0.5;
	}
	return result;
}

vec3 faceDirection(vec2 uv)
{
	if (u_Face == 0) return normalize(vec3(1, -uv.y, -uv.x));
	if (u_Face == 1) return normalize(vec3(-1, -uv.y, uv.x));
	if (u_Face == 2) return normalize(vec3(uv.x, 1, uv.y));
	if (u_Face == 3) return normalize(vec3(uv.x, -1, -uv.y));
	if (u_Face == 4) return normalize(vec3(uv.x, -uv.y, 1));
	return normalize(vec3(-uv.x, -uv.y, -1));
}

void main()
{
	vec3 d = faceDirection(2.0 * gl_FragCoord.xy / u_Resolution - 1.0);
	vec3 sun = normalize(vec3(-0.48, 0.085, -0.87));
	float alignment = max(dot(d, sun), 0.0);
	float height = max(d.y, 0.0);
	float horizon = exp(-height * 7.0);
	float glow = pow(alignment, 12.0);

	// Violet zenith, rose middle sky and an amber atmospheric horizon.
	vec3 color = mix(vec3(0.10, 0.08, 0.23), vec3(0.50, 0.22, 0.30), exp(-height * 2.8));
	color = mix(color, mix(vec3(0.70, 0.32, 0.26), vec3(1.35, 0.57, 0.17), glow), horizon);
	color += vec3(1.0, 0.35, 0.08) * pow(alignment, 48.0) * 0.55;
	float disk = smoothstep(cos(0.018), cos(0.013), dot(d, sun));
	color += disk * vec3(4.0, 2.3, 0.85);

	// Direction-space noise is continuous across all six cubemap boundaries.
	float wisps = fbm(d * vec3(8.0, 38.0, 8.0) + vec3(41.0, 2.0, 13.0));
	float highCloud = smoothstep(0.53, 0.72, wisps) * smoothstep(0.04, 0.28, d.y);
	color = mix(color, vec3(0.72, 0.43, 0.48) + glow * vec3(0.40, 0.15, 0.04), highCloud * 0.45);

	vec3 cloudPoint = d * vec3(5.0, 15.0, 5.0) + vec3(5.4, 11.0, 2.0);
	float shape = fbm(cloudPoint + fbm(cloudPoint * 0.7) * 2.0);
	float density = smoothstep(0.46, 0.68, shape);
	density *= smoothstep(0.015, 0.10, d.y) * (1.0 - smoothstep(0.65, 0.95, d.y));
	float rim = smoothstep(0.45, 0.54, shape) * (1.0 - smoothstep(0.54, 0.65, shape));
	vec3 cloudColor = mix(vec3(0.17, 0.12, 0.23), vec3(0.47, 0.24, 0.28), horizon);
	cloudColor += glow * vec3(0.65, 0.23, 0.06) + rim * pow(alignment, 4.0) * vec3(0.85, 0.36, 0.10);
	color = mix(color, cloudColor, density * 0.90);

	// Distant layered silhouettes sit below the playable arena's horizon.
	vec2 azimuth = d.xz / max(length(d.xz), 0.001);
	float ridge = -0.018 + 0.025 * noise(vec3(azimuth * 9.0, 3.0))
		+ 0.016 * noise(vec3(azimuth * 27.0, 7.0));
	float farMountain = 1.0 - smoothstep(ridge - 0.003, ridge + 0.003, d.y);
	color = mix(color, vec3(0.23, 0.16, 0.25), farMountain);
	float nearRidge = -0.045 + 0.036 * noise(vec3(azimuth * 13.0, 19.0));
	color = mix(color, vec3(0.10, 0.09, 0.15), 1.0 - smoothstep(nearRidge - 0.002, nearRidge + 0.002, d.y));
	color = mix(color, vec3(0.055, 0.045, 0.08), smoothstep(0.08, 0.75, -d.y));
	FragColor = vec4(color, 1.0);
}
