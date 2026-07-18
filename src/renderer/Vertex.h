#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.h>
#include <array>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec3 tangent;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, normal);

        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Vertex, tangent);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color &&
            texCoord == other.texCoord && normal == other.normal && tangent == other.tangent;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            size_t h1 = hash<glm::vec3>()(vertex.pos);
            size_t h2 = hash<glm::vec3>()(vertex.color);
            size_t h3 = hash<glm::vec2>()(vertex.texCoord);
            size_t h4 = hash<glm::vec3>()(vertex.normal);
            size_t h5 = hash<glm::vec3>()(vertex.tangent);
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1) ^ (h4 << 2) ^ (h5 << 3);
        }
    };
}

#define MAX_FRAMES_IN_FLIGHT 2

#define MAX_DIR_LIGHTS 2
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4

struct DirectionalLight {
    alignas(16) glm::vec3 direction;   
    alignas(16) glm::vec3 color;
};

struct PointLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 attenuation;   
};

struct SpotLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 attenuation;       
    alignas(16) glm::vec2 cutOff;            
};

#define NUM_CASCADES 4

struct FrameUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 cascadeViewProj[NUM_CASCADES]; 
    alignas(16) glm::vec4 cascadeSplits;
    alignas(16) glm::mat4 spotLightMatrices[MAX_SPOT_LIGHTS];
    alignas(16) glm::mat4 pointLightMatrices[MAX_POINT_LIGHTS][6];   
    alignas(16) glm::vec4 pointLightFarPlane;
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
    alignas(16) glm::vec4 viewPosAndCounts;
    alignas(16) glm::ivec4 lightCounts;
};

struct ObjectUBO {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 normalMatrix;
};