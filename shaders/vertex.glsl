#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aColor;

out vec3 ourColor;
out vec3 fragNormal;
out vec2 fragTexCoords;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;


void main()
{
    gl_Position   = projection * view * model * vec4(aPos, 1.0);
    ourColor      = aColor;
    fragNormal    = aNormal;
    fragTexCoords = aTexCoords;
}