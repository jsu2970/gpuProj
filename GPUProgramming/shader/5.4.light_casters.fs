#version 330 core
out vec4 FragColor;

uniform float shininess;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;

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
        vec3 lampDiffuse = lampLights[i].diffuse * lampDiff * texture(texture_diffuse1, TexCoords).rgb;  // diff값 계산

        float lampDistance = length(lampLights[i].position - FragPos);  // 유저로부터의 거리 계산

        // 감쇠 계산
        float lampAttenuation = 1.0 / (lampLights[i].constant + lampLights[i].linear * lampDistance + lampLights[i].quadratic * lampDistance * lampDistance);
        lampDiffuse *= lampAttenuation;

        result += lampDiffuse;
    }

    if (isEmissive)  // 전등 발광 처리
    {
        //vec3 emissionColor = texture(texture_diffuse1, TexCoords).rgb;
        //result += emissionColor * 3.0;

        vec3 eerieColor = vec3(0.25, 1.0, 0.75);
        result += eerieColor * 0.6;
    }

    FragColor = vec4(result, 1.0);
    //FragColor = texture(texture_diffuse1, TexCoords);
} 