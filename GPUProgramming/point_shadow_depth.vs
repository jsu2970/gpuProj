#version 330 core

layout (location = 0) in vec3 aPos;  // 물건의 원래 위치

uniform mat4 model;

out vec3 WorldPos;

void main()
{
    // 월드 좌표로 변환만 함. 실제 6방향 투영은 geometry shader에서 처리
    WorldPos = model * vec4(aPos, 1.0);
}