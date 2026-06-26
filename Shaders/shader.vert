#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in uint inBlockID;

layout(push_constant) uniform PushConstants {
    mat4 vp;
} pc;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out flat uint fragBlockID;

void main() {
    gl_Position = pc.vp * vec4(inPosition, 1.0);
    fragPos = inPosition;
    fragNormal = inNormal;
    fragBlockID = inBlockID;
}