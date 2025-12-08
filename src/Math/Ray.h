#pragma once

#include <glm/glm.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    
    Ray() = default;
    Ray(const glm::vec3& origin, const glm::vec3& direction)
        : origin(origin), direction(glm::normalize(direction)) {}
    
    glm::vec3 at(float t) const {
        return origin + t * direction;
    }
    
    // Ray-AABB intersection test
    bool intersectAABB(const glm::vec3& min, const glm::vec3& max, float& tMin, float& tMax) const;
};
