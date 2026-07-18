#version 450

layout(push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;
    vec4 lightPosFarPlane; // xyz = light world pos, w = far plane
} pc;

layout(set = 0, binding = 0) uniform ObjectUBO {
    mat4 model;
    mat4 normalMatrix;
} object;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = object.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    gl_Position = pc.lightSpaceMatrix * worldPos;
}