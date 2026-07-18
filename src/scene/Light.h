#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "GameObject.h"

enum class LightType {
    Directional,
    Point,
    Spot
};

class Light {
public:
    LightType type = LightType::Point;

    glm::vec3 position = glm::vec3(0.0f);     // used by Point, Spot
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);   // used by Directional, Spot
    glm::vec3 color = glm::vec3(1.0f);

    // Point/Spot attenuation
    glm::vec3 attenuation = glm::vec3(1.0f, 0.09f, 0.032f);   // constant, linear, quadratic

    // Spot-only cone angles, in degrees for ease of authoring
    float innerConeAngleDegrees = 12.5f;
    float outerConeAngleDegrees = 17.5f;

    GameObject* parent = nullptr;

    glm::vec2 getCutOff() const {
        return glm::vec2(
            glm::cos(glm::radians(innerConeAngleDegrees)),
            glm::cos(glm::radians(outerConeAngleDegrees))
        );
    }

    glm::vec3 getWorldPosition() const {           
        if (parent) {
            glm::vec4 worldPos = parent->getWorldMatrix() * glm::vec4(position, 1.0f);
            return glm::vec3(worldPos);
        }
        return position;
    }

    glm::vec3 getWorldDirection() const {          
        if (parent) {
            glm::mat3 rotationOnly = glm::mat3(parent->getWorldMatrix());   // see note below
            return glm::normalize(rotationOnly * direction);
        }
        return glm::normalize(direction);
    }
};