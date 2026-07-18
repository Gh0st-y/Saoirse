#pragma once

#include <glm/glm.hpp>
#include <array>

struct Plane {
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    float distance = 0.0f;   // distance from origin along normal

    float getSignedDistanceToPoint(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }
};

class Frustum {
public:
    std::array<Plane, 6> planes;   // 0=left, 1=right, 2=bottom, 3=top, 4=near, 5=far

    static Frustum fromViewProjection(const glm::mat4& viewProj) {
        Frustum frustum;

        glm::mat4 m = glm::transpose(viewProj);   // transpose makes row access below correspond to matrix rows

        frustum.planes[0] = normalizePlane(m[3] + m[0]);   // left
        frustum.planes[1] = normalizePlane(m[3] - m[0]);   // right
        frustum.planes[2] = normalizePlane(m[3] + m[1]);   // bottom
        frustum.planes[3] = normalizePlane(m[3] - m[1]);   // top
        frustum.planes[4] = normalizePlane(m[3] + m[2]);   // near
        frustum.planes[5] = normalizePlane(m[3] - m[2]);   // far

        return frustum;
    }

    bool isSphereVisible(const glm::vec3& center, float radius) const {   
        for (const auto& plane : planes) {
            if (plane.getSignedDistanceToPoint(center) < -radius) {
                return false;   // entirely behind this plane — definitely not visible
            }
        }
        return true;   // not excluded by any plane — treat as visible (may be a false positive, never a false negative)
    }

private:
    static Plane normalizePlane(const glm::vec4& v) {
        Plane plane;
        glm::vec3 normal(v.x, v.y, v.z);
        float length = glm::length(normal);

        plane.normal = normal / length;
        plane.distance = v.w / length;
        return plane;
    }
};