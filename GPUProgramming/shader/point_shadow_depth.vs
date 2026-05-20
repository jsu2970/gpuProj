#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 shadowMatrix;

out vec3 FragPos;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    gl_Position = shadowMatrix * worldPos;
}