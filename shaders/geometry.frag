#version 450

layout(set = 1, binding = 1) uniform sampler2D texSampler;
layout(set = 1, binding = 2) uniform sampler2D normalMapSampler;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;

layout(location = 0) out vec4 outPosition;   // G-buffer attachment 0
layout(location = 1) out vec4 outNormal;     // G-buffer attachment 1
layout(location = 2) out vec4 outAlbedo;     // G-buffer attachment 2

void main() {
    vec3 normalMapSample = texture(normalMapSampler, fragTexCoord).rgb;
    vec3 tangentSpaceNormal = normalMapSample * 2.0 - 1.0;

    mat3 TBN = mat3(fragTangent, fragBitangent, fragNormal);
    vec3 worldNormal = normalize(TBN * tangentSpaceNormal);

    outPosition = vec4(fragWorldPos, 1.0);
    outNormal = vec4(worldNormal, 0.0);
    outAlbedo = texture(texSampler, fragTexCoord);
}