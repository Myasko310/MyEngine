#version 330 core

in vec2  v_TexCoords;
in vec4  v_Color;

uniform sampler2D u_Texture;
uniform bool      u_UseTexture;

out vec4 FragColor;

void main()
{
	vec4 color = v_Color;

	if (u_UseTexture)
	{
		vec4 texSample = texture(u_Texture, v_TexCoords);
		color *= texSample;
	}
	else
	{
		// Soft circular billboard: fade out towards the edge
		float dist = length(v_TexCoords - 0.5) * 2.0; // 0 at centre, 1 at edge
		float alpha = 1.0 - smoothstep(0.6, 1.0, dist);
		color.a *= alpha;
	}

	if (color.a < 0.01)
		discard;

	FragColor = color;
}
