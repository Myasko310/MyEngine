#version 330 core

in vec2 v_TexCoords;
out vec4 FragColor;

uniform sampler2D u_Image;
uniform bool u_Horizontal;

// 9-tap Gaussian weights
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
	vec2 texelSize = 1.0 / vec2(textureSize(u_Image, 0));
	vec3 result = texture(u_Image, v_TexCoords).rgb * weights[0];

	if (u_Horizontal)
	{
		for (int i = 1; i < 5; ++i)
		{
			result += texture(u_Image, v_TexCoords + vec2(texelSize.x * i, 0.0)).rgb * weights[i];
			result += texture(u_Image, v_TexCoords - vec2(texelSize.x * i, 0.0)).rgb * weights[i];
		}
	}
	else
	{
		for (int i = 1; i < 5; ++i)
		{
			result += texture(u_Image, v_TexCoords + vec2(0.0, texelSize.y * i)).rgb * weights[i];
			result += texture(u_Image, v_TexCoords - vec2(0.0, texelSize.y * i)).rgb * weights[i];
		}
	}

	FragColor = vec4(result, 1.0);
}
