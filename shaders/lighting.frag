#version 450

#define MAX_DIR_LIGHTS 2
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
#define NUM_CASCADES 4
#define SPOT_SHADOW_SIZE 1024
#define POINT_SHADOW_SIZE 512

struct DirectionalLight { vec3 direction; vec3 color; };
struct PointLight { vec3 position; vec3 color; vec3 attenuation; };
struct SpotLight { vec3 position; vec3 direction; vec3 color; vec3 attenuation; vec2 cutOff; };

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 cascadeViewProj[NUM_CASCADES];
    vec4 cascadeSplits;
    mat4 spotLightMatrices[MAX_SPOT_LIGHTS];
    mat4 pointLightMatrices[MAX_POINT_LIGHTS][6];   // <-- NEW
    vec4 pointLightFarPlane;                          // <-- NEW
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
    vec4 viewPosAndCounts;
    ivec4 lightCounts;
} frame;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputPosition;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputNormal;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput inputAlbedo;

layout(set = 1, binding = 3) uniform sampler2DShadow shadowMaps[NUM_CASCADES];
layout(set = 1, binding = 4) uniform sampler2DShadow spotShadowMaps[MAX_SPOT_LIGHTS];
layout(set = 1, binding = 5) uniform samplerCube pointShadowMaps[MAX_POINT_LIGHTS];   // <-- NEW

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

const float ambientStrength = 0.1;
const float specularStrength = 0.5;
const float shininess = 32.0;
const float CASCADE_BLEND_RANGE = 0.1;

float sampleShadowPCF(sampler2DShadow shadowMap, vec3 projCoords, float texelSize) {
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec3 offset = vec3(vec2(x, y) * texelSize, 0.0);
            shadow += texture(shadowMap, projCoords + offset).r;
        }
    }
    return shadow / 9.0;
}

int selectCascade(vec3 worldPos) {
    vec4 viewSpacePos = frame.view * vec4(worldPos, 1.0);
    float depth = -viewSpacePos.z;

    for (int i = 0; i < NUM_CASCADES; i++) {
        if (depth < frame.cascadeSplits[i]) return i;
    }
    return NUM_CASCADES - 1;
}

float calcShadow(vec3 worldPos, vec3 normal) {
    int cascadeIdx = selectCascade(worldPos);

    vec4 viewSpacePos = frame.view * vec4(worldPos, 1.0);
    float depth = -viewSpacePos.z;

    float cascadeStart = (cascadeIdx == 0) ? 0.0 : frame.cascadeSplits[cascadeIdx - 1];
    float cascadeEnd = frame.cascadeSplits[cascadeIdx];
    float cascadeRange = cascadeEnd - cascadeStart;
    float blendStart = cascadeEnd - cascadeRange * CASCADE_BLEND_RANGE;

    float normalOffsetScale = 0.05 + cascadeIdx * 0.05;
    vec3 offsetPos = worldPos + normal * normalOffsetScale;

    vec4 lightSpacePos = frame.cascadeViewProj[cascadeIdx] * vec4(offsetPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float texelSize = 1.0 / float(cascadeIdx == 0 ? 4096 : (cascadeIdx <= 2 ? 2048 : 1024));
    float shadow = (projCoords.z > 1.0) ? 1.0 : sampleShadowPCF(shadowMaps[cascadeIdx], projCoords, texelSize);

    if (depth > blendStart && cascadeIdx < NUM_CASCADES - 1) {
        float blendFactor = (depth - blendStart) / (cascadeRange * CASCADE_BLEND_RANGE);
        int nextCascade = cascadeIdx + 1;
        vec3 nextOffsetPos = worldPos + normal * (0.05 + nextCascade * 0.05);
        vec4 nextLightSpacePos = frame.cascadeViewProj[nextCascade] * vec4(nextOffsetPos, 1.0);
        vec3 nextProjCoords = nextLightSpacePos.xyz / nextLightSpacePos.w;
        nextProjCoords.xy = nextProjCoords.xy * 0.5 + 0.5;
        float nextTexelSize = 1.0 / float(nextCascade == 0 ? 4096 : (nextCascade <= 2 ? 2048 : 1024));
        float nextShadow = (nextProjCoords.z > 1.0) ? 1.0 : sampleShadowPCF(shadowMaps[nextCascade], nextProjCoords, nextTexelSize);
        shadow = mix(shadow, nextShadow, blendFactor);
    }

    return shadow;
}

float calcSpotShadow(int spotIdx, vec3 worldPos, vec3 normal) {
    vec3 offsetPos = worldPos + normal * 0.05;
    vec4 lightSpacePos = frame.spotLightMatrices[spotIdx] * vec4(offsetPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) return 1.0;

    float texelSize = 1.0 / float(SPOT_SHADOW_SIZE);
    return sampleShadowPCF(spotShadowMaps[spotIdx], projCoords, texelSize);
}

float calcPointShadow(int pointIdx, vec3 worldPos, vec3 lightPos) {
    vec3 fragToLight = worldPos - lightPos;
    float currentDepth = length(fragToLight);   // actual world-space distance to light

    // Sample the cubemap using the direction vector — Vulkan selects the correct face automatically
    float closestDepth = texture(pointShadowMaps[pointIdx], fragToLight).r;

    // Stored depth is normalized [0,1] by the far plane — convert back to world-space distance
    closestDepth *= frame.pointLightFarPlane.x;

    float bias = 0.15;
    return (currentDepth - bias > closestDepth) ? 0.0 : 1.0;
}

vec3 calcDirLight(DirectionalLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    return diff * light.color + specularStrength * spec * light.color;   // no ambient here
}

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    return (diff * light.color + specularStrength * spec * light.color) * attenuation;   // no ambient here
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff.x - light.cutOff.y;
    float intensity = clamp((theta - light.cutOff.y) / epsilon, 0.0, 1.0);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    return (diff * light.color + specularStrength * spec * light.color) * attenuation * intensity;   // no ambient here
}

void main() {
    vec3 fragWorldPos = subpassLoad(inputPosition).rgb;
    vec3 normal = normalize(subpassLoad(inputNormal).rgb);
    vec3 albedo = subpassLoad(inputAlbedo).rgb;

    vec3 viewPos = frame.viewPosAndCounts.xyz;
    vec3 viewDir = normalize(viewPos - fragWorldPos);

    int numDir = frame.lightCounts.x;
    int numPoint = frame.lightCounts.y;
    int numSpot = frame.lightCounts.z;

    float shadow = calcShadow(fragWorldPos, normal);

    vec3 result = vec3(0.0);

    // Global ambient — not shadowed, just a flat contribution from every active light's color
    for (int i = 0; i < numDir; i++)   result += ambientStrength * frame.dirLights[i].color;
    for (int i = 0; i < numPoint; i++) result += ambientStrength * frame.pointLights[i].color;
    for (int i = 0; i < numSpot; i++)  result += ambientStrength * frame.spotLights[i].color;

    for (int i = 0; i < numDir; i++) {
        result += calcDirLight(frame.dirLights[i], normal, viewDir) * shadow;
    }

    for (int i = 0; i < numPoint; i++) {
        float pointShadow = calcPointShadow(i, fragWorldPos, frame.pointLights[i].position);
        result += calcPointLight(frame.pointLights[i], normal, fragWorldPos, viewDir) * pointShadow;
    }

    for (int i = 0; i < numSpot; i++) {
        float spotShadow = calcSpotShadow(i, fragWorldPos, normal);
        result += calcSpotLight(frame.spotLights[i], normal, fragWorldPos, viewDir) * spotShadow;
    }

    outColor = vec4(result * albedo, 1.0);
}