#version 330 core

in  vec3 ourColor;
in  vec3 fragNormal;
in  vec2 fragTexCoords;

out vec4 FragColor;

uniform vec3 objectColor;

void main()
{
    // Vertex color multiplied by object tint
    FragColor = vec4(ourColor * objectColor, 1.0);
}