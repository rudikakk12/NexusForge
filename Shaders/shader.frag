#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Egy nagyon alap, hamis "napfény" megvilágítás, hogy térbelinek tűnjenek a kockák
    vec3 lightDir = normalize(vec3(0.5, -1.0, 0.5));
    float diff = max(dot(fragNormal, -lightDir), 0.2); // 0.2 a minimum fény (ambient)

    vec3 finalColor = fragColor * diff;
    outColor = vec4(finalColor, 1.0);
}