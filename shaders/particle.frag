#version 330 core

in vec2  v_TexCoords;
in vec4  v_Color;
flat in vec2 v_MarkAge;

uniform sampler2D u_Texture;
uniform bool      u_UseTexture;
uniform bool      u_PressureTrail;

out vec4 FragColor;

void main()
{
	vec4 color = v_Color;

	if (u_UseTexture)
	{
		vec4 texSample = texture(u_Texture, v_TexCoords);
		color *= texSample;
	}
	else if (u_PressureTrail && v_MarkAge.x > 0.5)
	{
		vec2 q = v_TexCoords*2.0-1.0;
		float d;
		if (v_MarkAge.x < 1.5)
		{
			float x = v_TexCoords.x;
			float cel = floor(min(v_MarkAge.y,0.999)*3.0);
			float width = sin(3.14159*x)*(0.28+0.48*x);
			float y = q.y - (0.24+0.05*cel)*(1.0-q.x*q.x);
			d = abs(y)-width;
			float split = (0.62-x)*0.23 - abs(y+0.12*width);
			d = max(d,split);
			float ridge = 1.0-smoothstep(0.04,0.18,abs(y-0.32*width));
			color.rgb = mix(color.rgb*0.78,vec3(0.94,1.0,1.0),ridge);
		}
		else if (v_MarkAge.x < 2.5)
		{
			float angle = atan(q.y,q.x+0.00001);
			float rays = pow(abs(cos(angle*3.0+0.28*sin(angle*5.0))),8.0);
			float edge = 0.27+0.68*rays;
			float r = length(q);
			d = r-edge;
			if (v_MarkAge.y >= 0.36) d = max(d,max(edge*0.71-r,0.10-sin(angle*5.0+0.7)));
			color.rgb = mix(color.rgb,vec3(1.0),1.0-smoothstep(0.16,0.35,r));
		}
		else
		{
			float age = clamp(v_MarkAge.y,0.0,1.0);
			float turn = 0.95*age;
			vec2 curl = mat2(cos(turn),-sin(turn),sin(turn),cos(turn))*q;
			float angle = atan(curl.y,curl.x+0.00001);
			float span = mix(1.6,2.55,age);
			float tip = clamp(abs(angle)/span,0.0,1.0);
			float width = mix(0.14,0.055,age)*(1.0-tip*tip);
			float rim = length(curl)-(0.69+0.08*angle);
			d = max(abs(rim)-width,0.65*(abs(angle)-span));
			float highlight = 1.0-smoothstep(0.015,0.07,abs(rim+width*0.45));
			color.rgb = mix(color.rgb,vec3(0.91,1.0,1.0),highlight);
		}
		float aa = max(fwidth(d),0.008);
		color.a *= 1.0-smoothstep(-aa,aa,d);
	}
	else
	{
		// Soft circular billboard: fade out towards the edge
		float dist = length(v_TexCoords - 0.5) * 2.0; // 0 at centre, 1 at edge
		float alpha = 1.0 - smoothstep(0.6, 1.0, dist);
		if (u_PressureTrail)
		{
			float core = exp(-12.0 * dist * dist);
			float halo = exp(-2.8 * dist * dist);
			color.rgb = mix(color.rgb, vec3(0.88, 1.0, 1.0), 0.42 * core);
			alpha *= 0.62 * core + 0.38 * halo;
		}
		color.a *= alpha;
	}

	if (color.a < 0.01)
		discard;

	FragColor = color;
}
