#version 450

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
} frame;

layout(set = 1, binding = 0) uniform ObjectUBO {
    mat4 model;
    mat4 normalMatrix;
} object;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec3 fragBitangent;

void main() {
    vec4 worldPos = object.model * vec4(inPosition, 1.0);
    gl_Position = frame.proj * frame.view * worldPos;

    fragWorldPos = worldPos.xyz;
    vec3 N = normalize(mat3(object.normalMatrix) * inNormal);
    vec3 T = normalize(mat3(object.normalMatrix) * inTangent);
    // Re-orthogonalize T against N using Gram-Schmidt — Assimp's tangents are good
    // but floating point and non-uniform scale can make them slightly non-perpendicular
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    fragNormal    = N;
    fragTangent   = T;
    fragBitangent = B;
    fragTexCoord = inTexCoord;
}