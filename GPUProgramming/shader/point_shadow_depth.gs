#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

in vec3 WorldPos[];

uniform mat4 shadowMatrices[6];  // 전등 기준 6방향 카메라 행렬임

out vec4 FragPos;

void main()
{
    // 삼각형 하나를 cubemap 6개 면에 모두 보냄
    for (int face = 0; face < 6; face++) {
        gl_Layer = face;  // 이번 삼각형은 cubeamp의 몇 번째 카메라 화면에 그릴지 결정함. 0이면 +X, 1이면 -X ...

        // 삼각형 정점 3개를 처리함
        for (int i = 0; i < 3; i++) {
            FragPos = WorldPos[i];  // vs에서 넘어온 값으로 현재 정점의 실제 월드 좌표를 저장함
            gl_Position = shadowMatrices[face] * vec4(WorldPos[i], 1.0);  // 현재 전등 카메라(face방향)에서 보면 화면 어디에 보이는지 계산함 (벽이 전등 오른쪽에 있으면 +x카메라엔 크게 보이지만 -x는 거의 안보임)

            EmitVertex();  // 계산한 정점을 하나 출력
        }

        EndPrimitive();  // 정점들을 모아서 삼각형 하나를 처리함
    }
}