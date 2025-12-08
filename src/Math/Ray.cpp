#include "Ray.h"
#include <algorithm>

bool Ray::intersectAABB(const glm::vec3& min, const glm::vec3& max, float& tMin, float& tMax) const {
    tMin = 0.0f;
    tMax = std::numeric_limits<float>::max();
    
    for (int i = 0; i < 3; ++i) {
        if (std::abs(direction[i]) < 1e-8f) {
            // Ray is parallel to slab
            if (origin[i] < min[i] || origin[i] > max[i]) {
                return false;
            }
        } else {
            float ood = 1.0f / direction[i];
            float t1 = (min[i] - origin[i]) * ood;
            float t2 = (max[i] - origin[i]) * ood;
            
            if (t1 > t2) std::swap(t1, t2);
            
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            
            if (tMin > tMax) {
                return false;
            }
        }
    }
    
    return true;
}
