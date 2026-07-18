#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../renderer/Mesh.h"
#include "../renderer/Texture.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

class GameObject {
public:
    Mesh* mesh = nullptr;
    Texture* texture = nullptr;
    Texture* normalMap = nullptr;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    GameObject* parent = nullptr;                  
    std::vector<GameObject*> children;             

    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers{};
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uniformBufferMemories{};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> uniformBuffersMapped{};
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};

    glm::mat4 getLocalMatrix() const {              
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = glm::rotate(model, rotation.x, glm::vec3(1, 0, 0));
        model = glm::rotate(model, rotation.y, glm::vec3(0, 1, 0));
        model = glm::rotate(model, rotation.z, glm::vec3(0, 0, 1));
        model = glm::scale(model, scale);
        return model;
    }

    glm::mat4 getWorldMatrix() const {              
        if (parent) {
            return parent->getWorldMatrix() * getLocalMatrix();
        }
        return getLocalMatrix();
    }

    void setParent(GameObject* newParent) {          
        if (parent) {
            auto& siblings = parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }
        parent = newParent;
        if (newParent) {
            newParent->children.push_back(this);
        }
    }

    glm::vec3 getWorldBoundingSphereCenter() const {
        glm::vec4 worldCenter = getWorldMatrix() * glm::vec4(mesh->boundingSphereCenter, 1.0f);
        return glm::vec3(worldCenter);
    }

    float getWorldBoundingSphereRadius() const {
        // account for scale — use the largest scale component, since a sphere must remain
        // conservative (large enough to cover) even under non-uniform scaling
        float maxScale = std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });
        return mesh->boundingSphereRadius * maxScale;
    }
};