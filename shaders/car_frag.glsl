#version 430 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 uCameraPos;
uniform vec3 uLightPos;
uniform vec3 uLightColor = vec3(3.5, 3.5, 3.5);
uniform vec3 uBaseColor = vec3(0.08, 0.08, 0.12);
uniform float uMetallic = 1.0;
uniform float uRoughness = 0.03;
uniform float uClearCoat = 1.0;
uniform float uTime;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / max(denom, 0.001);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(Normal);
    vec3 V = normalize(uCameraPos - FragPos);
    vec3 L = normalize(uLightPos - FragPos);
    vec3 H = normalize(V + L);

    vec3 F0 = mix(vec3(0.04), uBaseColor, uMetallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(N, H, uRoughness);

    vec3 specular = NDF * F * uLightColor * 2.0;
    vec3 diffuse = (1.0 - F) * (1.0 - uMetallic) * uBaseColor / PI * uLightColor;

    float clearCoat = uClearCoat * pow(1.0 - max(dot(V, N), 0.0), 5.0);
    vec3 color = (diffuse + specular) * max(dot(N, L), 0.0) + clearCoat * uLightColor * 2.0;
    color += NDF * 0.5 * uLightColor; 

    FragColor = vec4(color, 1.0);
}