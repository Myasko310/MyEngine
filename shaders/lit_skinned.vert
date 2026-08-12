#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec2 a_TexCoords;
layout(location = 4) in ivec4 a_BoneIDs;
layout(location = 5) in vec4 a_BoneWeights;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_LightSpace;

const int MAX_BONES = 100;
uniform mat4 u_BoneMatrices[MAX_BONES];

out vec3 v_Position;
out vec3 v_Color;
out vec3 v_Normal;
out vec2 v_TexCoords;
out vec4 v_FragPosLightSpace;

void main()
{
	mat4 skinMatrix = mat4(0.0);
	float totalWeight = 0.0;

	for (int i = 0; i < 4; i++)
	{
		int boneID = a_BoneIDs[i];
		float weight = a_BoneWeights[i];

		if (boneID < 0 || weight <= 0.0)
			continue;

		skinMatrix = skinMatrix + u_BoneMatrices[boneID] * weight;
		totalWeight = totalWeight + weight;
	}

	if (totalWeight <= 0.0001)
		skinMatrix = mat4(1.0);

	vec4 skinnedPosition = skinMatrix * vec4(a_Position, 1.0);
	vec3 skinnedNormal = mat3(skinMatrix) * a_Normal;

	v_Position = vec3(u_Model * skinnedPosition);
	v_Normal = mat3(transpose(inverse(u_Model))) * skinnedNormal;
	v_Color = a_Color;
	v_TexCoords = a_TexCoords;
	v_FragPosLightSpace = u_LightSpace * vec4(v_Position, 1.0);
	gl_Position = u_Projection * u_View * vec4(v_Position, 1.0);
}
