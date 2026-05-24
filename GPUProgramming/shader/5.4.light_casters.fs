#version 330 core
out vec4 FragColor;

uniform float shininess;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;

// 그림자를 그리기 위한 변수
uniform samplerCube depthMaps[2];
uniform vec3 shadowLightPositions[2];
uniform int shadowLampIndices[2];
uniform float far_plane;

// 손전등 계산을 위한 구조체
struct Light {
    vec3 position;  
    vec3 direction;
    float cutOff;
    float outerCutOff;
  
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
	
    float constant;
    float linear;
    float quadratic;
};
uniform Light light;

#define NR_LAMP_LIGHTS 64  // 최대로 받을 점광원의 개수
uniform int lampLightCount;  // 실제로 사용 중인 전등 개수

// 맵의 전등을 표현하기 위한 구조체
struct PointLight {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};
uniform PointLight lampLights[NR_LAMP_LIGHTS];

in vec3 FragPos;  
in vec3 Normal;  
in vec2 TexCoords;
  
uniform vec3 viewPos;
uniform bool isEmissive;  // 발광 여부
uniform vec3 fogColor;    
uniform float fogDensity;

// 현재 픽셀이 그림자인지 계산하는 함수 (PCF 적용)
float ShadowCalculation(vec3 fragPos, int shadowSlot)
{
    vec3 fragToLight = fragPos - shadowLightPositions[shadowSlot];  // 전등 -> 픽셀 방향 벡터
    float currentDepth = length(fragToLight);  // 실제 거리 계산

    float shadow = 0.0;  // 몇 개 샘플이 그림자인지 세는 변수
    float bias = 0.08;  // shadow acne 방지 (자기 자신을 그림자로 판단하는 문제)

    // PCF 샘플 방향들
    vec3 sampleOffsetDirections[20] = vec3[]
    (
        vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
        vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
        vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
        vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
        vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0,  1, -1), vec3( 0, -1, -1)
    );

    int samples = 20;  // 주변 방향 20개 검사

    // 카메라와 현재픽셀의 거리로, 카메라가 멀수록 그림자 경계를 조금 더 넓게 샘플링 (부드러워짐)
    float viewDistance = length(viewPos - fragPos); 
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 80.0;

    for (int i = 0; i < samples; i++)
    {
        // 현재 방향의 쉐도우 맵 깊이를 가져옴
        float closestDepth = texture(
            depthMaps[shadowSlot],
            fragToLight + sampleOffsetDirections[i] * diskRadius
        ).r;
        closestDepth *= far_plane;  // 정규화된 depth를 실제 거리로 복원함

        if (currentDepth - bias > closestDepth)  // 그림자를 판정함. 쉐도우 맵의 가장 가까운 벽보다 멀면 중간에 벽이 있으므로 그림자 판정
            shadow += 1.0;
    }

    shadow /= float(samples);  // 그림자 비율을 평균냄 (밝기가 달라짐)

    return shadow;  
}

void main()
{
    // ======================
    // 손전등 빛 계산
    // ======================

    // ambient
    vec3 ambient = light.ambient * texture(texture_diffuse1, TexCoords).rgb;
    
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * texture(texture_diffuse1, TexCoords).rgb;  
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    //vec3 specular = light.specular * spec * texture(texture_specular1, TexCoords).rgb;
    vec3 specular = light.specular * spec * vec3(0.2);
    
    // spotlight (soft edges)
    float theta = dot(lightDir, normalize(-light.direction)); 
    float epsilon = (light.cutOff - light.outerCutOff);
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    diffuse  *= intensity;
    specular *= intensity;
    
    // attenuation
    float distance    = length(light.position - FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    ambient  *= attenuation; 
    diffuse   *= attenuation;
    specular *= attenuation;   

    vec3 result = ambient + diffuse + specular;

    // ======================
    // 전등 point light 계산
    // ======================
    
    for (int i = 0; i < lampLightCount; i++)
    {
        vec3 lampDir = normalize(lampLights[i].position - FragPos);  
        float lampDiff = max(dot(norm, lampDir), 0.0); 

        // 전등의 ambient 계산 추가
        vec3 lampAmbient = lampLights[i].ambient * texture(texture_diffuse1, TexCoords).rgb;

        vec3 lampDiffuse = lampLights[i].diffuse * lampDiff * texture(texture_diffuse1, TexCoords).rgb;  // diff값 계산

        float lampDistance = length(lampLights[i].position - FragPos);  // 유저로부터의 거리 계산

        // 감쇠 계산
        float lampAttenuation = 1.0 / (lampLights[i].constant + lampLights[i].linear * lampDistance + lampLights[i].quadratic * lampDistance * lampDistance);
        lampAmbient *= lampAttenuation;
        lampDiffuse *= lampAttenuation;

        // 가까운 전등 2개에만 cube shadow 적용
        for (int s = 0; s < 2; s++) {
            if (i == shadowLampIndices[s]) {
                float shadow = ShadowCalculation(FragPos, s);

                lampDiffuse *= (1.0 - shadow);
            }
        }

        result += (lampAmbient + lampDiffuse);
    }

    if (isEmissive)  // 전등 발광 처리
    {
        //vec3 emissionColor = texture(texture_diffuse1, TexCoords).rgb;
        //result += emissionColor * 3.0;

        vec3 eerieColor = vec3(0.25, 1.0, 0.75);
        result += eerieColor * 0.6;
    }

    // ======================
    // 안개(Fog) 계산 추가
    // ======================
    float fogDist = length(viewPos - FragPos); // 카메라와 픽셀 사이의 거리 계산 [cite: 10, 14]
    
    // 지수 안개 공식: f = e^(- (distance * density)^2)
    float fogOffset = 2.0; // 2.0 유닛까지는 안개가 끼지 않음(안개 가시거리 조정)
    float adjustedDist = max(fogDist - fogOffset, 0.0);
    float fogFactor = exp(-pow(adjustedDist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // 최종 결과물과 안개 색상을 혼합
    // fogFactor가 1에 가까우면 원래 색상, 0에 가까우면 안개 색상이 보임
    result = mix(fogColor, result, fogFactor);

    FragColor = vec4(result, 1.0);
    //FragColor = texture(texture_diffuse1, TexCoords);
} 
