#pragma once

#include "Vertex.h"
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp> 

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    glm::vec3 boundingSphereCenter = glm::vec3(0.0f);  
    float boundingSphereRadius = 0.0f;                   

    void computeBoundingSphere() {
        if (vertices.empty()) return;

        // Step 1: center = average of all vertex positions (cheap, reasonably good approximation)
        glm::vec3 center(0.0f);
        for (const auto& v : vertices) center += v.pos;
        center /= static_cast<float>(vertices.size());

        // Step 2: radius = distance to the farthest vertex from that center
        float maxDistSq = 0.0f;
        for (const auto& v : vertices) {
            float distSq = glm::length2(v.pos - center);
            maxDistSq = std::max(maxDistSq, distSq);
        }

        boundingSphereCenter = center;
        boundingSphereRadius = std::sqrt(maxDistSq);
    }
};