#version 330 core

in vec3 FragPos;

uniform vec3 lightPos;
uniform float far_plane;

void main()
{
    float lightDistance = length(FragPos - lightPos);
    gl_FragDepth = lightDistance / far_plane;
}