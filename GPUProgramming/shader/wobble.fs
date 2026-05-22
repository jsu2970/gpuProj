#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse1;
uniform float time;

void main()
{
    vec2 uv = TexCoords;

    // 흐르는 왜곡
    uv.x += sin(uv.y * 25.0 + time * 3.0) * 0.02;
    uv.y += cos(uv.x * 20.0 + time * 2.5) * 0.02;

    vec4 tex = texture(texture_diffuse1, uv);

    // 초록값을 마스크처럼 사용
    float mask = tex.g;

    // 배경 제거
    if (mask < 0.05)
        discard;

    // 거의 검은색 형체
    vec3 color = vec3(0.01);

    // 가장자리 흐리게
    float fade =
        smoothstep(0.0, 0.15, uv.x) *
        smoothstep(0.0, 0.15, uv.y) *
        smoothstep(0.0, 0.15, 1.0 - uv.x) *
        smoothstep(0.0, 0.15, 1.0 - uv.y);

    FragColor = vec4(color, mask * fade);
}