#version 330 core

in vec2 v_TexCoords;
out float FragOcclusion;

uniform sampler2D u_SSAOInput;

void main()
{
	vec2 texelSize = 1.0 / vec2(textureSize(u_SSAOInput, 0));
	float result = 0.0;
	for (int x = -2; x <= 2; ++x)
		for (int y = -2; y <= 2; ++y)
			result += texture(u_SSAOInput, v_TexCoords + vec2(x, y) * texelSize).r;
	FragOcclusion = result / 25.0;
}
