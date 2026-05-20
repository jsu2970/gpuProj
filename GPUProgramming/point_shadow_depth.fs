#version 330 core

in vec4 FragPos;

uniform vec3 lightPos;  // 전등 위치
uniform float far_plane;  // 정규화를 위한 거리

void main()
{
    // 전등에서 현재 fragment까지의 거리
    float lightDistance = length(FragPos.xyz - lightPos);

    // 0~1 범위로 정규화해서 depth 값으로 저장
    lightDistance = lightDistance / far_plane;

    gl_FragDepth = lightDistance;
}