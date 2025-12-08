#pragma once

#include <glm/glm.hpp>
#include <array>

class Frustum {
public:
    Frustum() = default;
    ~Frustum() = default;

    void update(const glm::mat4& viewProj);
    bool isBoxVisible(const glm::vec3& min, const glm::vec3& max) const;

private:
    struct Plane {
        glm::vec3 normal;
        float distance;
        
        float distanceToPoint(const glm::vec3& point) const {
            return glm::dot(normal, point) + distance;
        }
    };
    
    std::array<Plane, 6> planes;
    
    void normalizePlane(Plane& plane);
};
