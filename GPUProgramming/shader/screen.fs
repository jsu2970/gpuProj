#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform float horrorAmount;
uniform int horrorMode;
uniform float time;

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

// post-processing을 위한 FS
void main()
{
    vec3 original = texture(screenTexture, TexCoords).rgb;  // 현재 화면 픽셀 색 가져오기
    vec3 horrorColor = original;  // 필터를 서서히 적용시키기 위해 후처리 전 원본색을 가져옴

    // 1번: 어둡고 붉은 공포 필터
    if (horrorMode == 1)
    {
        horrorColor.r *= 0.9;
        horrorColor.g *= 0.55;
        horrorColor.b *= 0.55;

        float gray = dot(horrorColor, vec3(0.299, 0.587, 0.114));
        horrorColor = mix(horrorColor, vec3(gray), 0.35);
    }

    // 2번: 낡은 CCTV / 노이즈 필터
    else if (horrorMode == 2)
    {
        float gray = dot(horrorColor, vec3(0.299, 0.587, 0.114));
        horrorColor = vec3(gray);

        float noise = rand(TexCoords * time) * 0.15;
        horrorColor += noise;

        float scanline = sin(TexCoords.y * 800.0) * 0.04;
        horrorColor -= scanline;
    }

    // 3번: 어두운 청록색 병원 느낌
    else if (horrorMode == 3)
    {
        horrorColor.r *= 0.55;
        horrorColor.g *= 0.85;
        horrorColor.b *= 0.8;

        float gray = dot(horrorColor, vec3(0.299, 0.587, 0.114));
        horrorColor = mix(horrorColor, vec3(gray), 0.25);
    }

    // 4번: 주변부 어둡게, 비네트 효과
    else if (horrorMode == 4)
    {
        vec2 center = TexCoords - vec2(0.5);
        float dist = length(center);

        float vignette = smoothstep(0.8, 0.25, dist);
        horrorColor *= vignette;

        horrorColor.r *= 0.9;
        horrorColor.g *= 0.65;
        horrorColor.b *= 0.65;
    }

    // 5번: 강한 정신왜곡 / 불안한 화면
    else if (horrorMode == 5)
    {
        vec2 offset;
        offset.x = sin(time * 8.0 + TexCoords.y * 40.0) * 0.003;
        offset.y = cos(time * 6.0 + TexCoords.x * 30.0) * 0.003;

        vec3 distorted = texture(screenTexture, TexCoords + offset).rgb;

        distorted.r = texture(screenTexture, TexCoords + offset + vec2(0.003, 0.0)).r;
        distorted.g = texture(screenTexture, TexCoords + offset).g;
        distorted.b = texture(screenTexture, TexCoords + offset - vec2(0.003, 0.0)).b;

        float gray = dot(distorted, vec3(0.299, 0.587, 0.114));
        horrorColor = mix(distorted, vec3(gray), 0.45);
    }
    
    vec3 finalColor = mix(original, horrorColor, horrorAmount);
    FragColor = vec4(finalColor, 1.0);
}


