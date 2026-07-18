#version 450

layout(push_constant) uniform LightSpaceMatrix {
    mat4 lightSpaceMatrix;
} pc;

layout(set = 0, binding = 0) uniform ObjectUBO {
    mat4 model;
    mat4 normalMatrix;
} object;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

void main() {
    gl_Position = pc.lightSpaceMatrix * object.model * vec4(inPosition, 1.0);
}