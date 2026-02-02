#include "CollisionDetection.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Physics {

// ============================================================================
// Utility Functions
// ============================================================================

glm::vec3 CollisionDetection::closestPointOnSegment(
    const glm::vec3& point,
    const glm::vec3& segStart, const glm::vec3& segEnd) {
    
    glm::vec3 segment = segEnd - segStart;
    float segLenSq = glm::dot(segment, segment);
    
    if (segLenSq < 1e-8f) {
        return segStart;
    }
    
    float t = std::clamp(glm::dot(point - segStart, segment) / segLenSq, 0.0f, 1.0f);
    return segStart + segment * t;
}

void CollisionDetection::closestPointsBetweenSegments(
    const glm::vec3& p0A, const glm::vec3& p1A,
    const glm::vec3& p0B, const glm::vec3& p1B,
    glm::vec3& closestA, glm::vec3& closestB) {
    
    glm::vec3 dA = p1A - p0A;
    glm::vec3 dB = p1B - p0B;
    glm::vec3 r = p0A - p0B;
    
    float a = glm::dot(dA, dA);
    float e = glm::dot(dB, dB);
    float f = glm::dot(dB, r);
    
    float s, t;
    
    if (a < 1e-8f && e < 1e-8f) {
        // Both segments are points
        closestA = p0A;
        closestB = p0B;
        return;
    }
    
    if (a < 1e-8f) {
        // First segment is a point
        s = 0.0f;
        t = std::clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = glm::dot(dA, r);
        if (e < 1e-8f) {
            // Second segment is a point
            t = 0.0f;
            s = std::clamp(-c / a, 0.0f, 1.0f);
        } else {
            // General case
            float b = glm::dot(dA, dB);
            float denom = a * e - b * b;
            
            if (std::abs(denom) > 1e-8f) {
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }
            
            t = (b * s + f) / e;
            
            if (t < 0.0f) {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    
    closestA = p0A + dA * s;
    closestB = p0B + dB * t;
}

glm::vec3 CollisionDetection::computeAABBContactNormal(const AABB& a, const AABB& b) {
    glm::vec3 centerA = a.getCenter();
    glm::vec3 centerB = b.getCenter();
    glm::vec3 extentsA = a.getExtents();
    glm::vec3 extentsB = b.getExtents();
    
    glm::vec3 d = centerB - centerA;
    glm::vec3 overlap = (extentsA + extentsB) - glm::abs(d);
    
    // Find axis of minimum penetration
    if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
        return glm::vec3(d.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
    } else if (overlap.y <= overlap.z) {
        return glm::vec3(0.0f, d.y > 0.0f ? 1.0f : -1.0f, 0.0f);
    } else {
        return glm::vec3(0.0f, 0.0f, d.z > 0.0f ? 1.0f : -1.0f);
    }
}

// ============================================================================
// Sphere vs Sphere
// ============================================================================
bool CollisionDetection::sphereSphere(
    const glm::vec3& centerA, float radiusA,
    const glm::vec3& centerB, float radiusB,
    Contact& contact) {
    
    glm::vec3 d = centerB - centerA;
    float distSq = glm::dot(d, d);
    float radiusSum = radiusA + radiusB;
    
    if (distSq > radiusSum * radiusSum) {
        return false;
    }
    
    float dist = std::sqrt(distSq);
    
    if (dist > 1e-6f) {
        contact.normal = d / dist;
        contact.penetration = radiusSum - dist;
        contact.point = centerA + contact.normal * (radiusA - contact.penetration * 0.5f);
    } else {
        // Spheres are at same position
        contact.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        contact.penetration = radiusSum;
        contact.point = centerA;
    }
    
    return true;
}

// ============================================================================
// Sphere vs AABB
// ============================================================================
bool CollisionDetection::sphereAABB(
    const glm::vec3& sphereCenter, float sphereRadius,
    const AABB& aabb,
    Contact& contact) {
    
    // Find closest point on AABB to sphere center
    glm::vec3 closest;
    closest.x = std::clamp(sphereCenter.x, aabb.min.x, aabb.max.x);
    closest.y = std::clamp(sphereCenter.y, aabb.min.y, aabb.max.y);
    closest.z = std::clamp(sphereCenter.z, aabb.min.z, aabb.max.z);
    
    glm::vec3 d = sphereCenter - closest;
    float distSq = glm::dot(d, d);
    
    if (distSq > sphereRadius * sphereRadius) {
        return false;
    }
    
    float dist = std::sqrt(distSq);
    
    if (dist > 1e-6f) {
        contact.normal = d / dist;
        contact.penetration = sphereRadius - dist;
        contact.point = closest;
    } else {
        // Sphere center is inside AABB
        glm::vec3 aabbCenter = aabb.getCenter();
        glm::vec3 halfExtents = aabb.getExtents();
        glm::vec3 diff = sphereCenter - aabbCenter;
        
        // Find axis of minimum penetration
        float minPen = halfExtents.x - std::abs(diff.x);
        contact.normal = glm::vec3(diff.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
        
        float penY = halfExtents.y - std::abs(diff.y);
        if (penY < minPen) {
            minPen = penY;
            contact.normal = glm::vec3(0.0f, diff.y > 0.0f ? 1.0f : -1.0f, 0.0f);
        }
        
        float penZ = halfExtents.z - std::abs(diff.z);
        if (penZ < minPen) {
            minPen = penZ;
            contact.normal = glm::vec3(0.0f, 0.0f, diff.z > 0.0f ? 1.0f : -1.0f);
        }
        
        contact.penetration = minPen + sphereRadius;
        contact.point = sphereCenter - contact.normal * sphereRadius;
    }
    
    return true;
}

// ============================================================================
// Sphere vs Box (OBB)
// ============================================================================
bool CollisionDetection::sphereBox(
    const glm::vec3& sphereCenter, float sphereRadius,
    const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
    const glm::mat3& boxRotation,
    Contact& contact) {
    
    // Transform sphere center to box local space
    glm::vec3 localCenter = glm::transpose(boxRotation) * (sphereCenter - boxCenter);
    
    // Find closest point in box local space
    glm::vec3 closest;
    closest.x = std::clamp(localCenter.x, -boxHalfExtents.x, boxHalfExtents.x);
    closest.y = std::clamp(localCenter.y, -boxHalfExtents.y, boxHalfExtents.y);
    closest.z = std::clamp(localCenter.z, -boxHalfExtents.z, boxHalfExtents.z);
    
    glm::vec3 d = localCenter - closest;
    float distSq = glm::dot(d, d);
    
    if (distSq > sphereRadius * sphereRadius) {
        return false;
    }
    
    float dist = std::sqrt(distSq);
    
    if (dist > 1e-6f) {
        glm::vec3 localNormal = d / dist;
        contact.normal = boxRotation * localNormal;
        contact.penetration = sphereRadius - dist;
        contact.point = boxCenter + boxRotation * closest;
    } else {
        // Sphere center inside box
        float minPen = boxHalfExtents.x - std::abs(localCenter.x);
        glm::vec3 localNormal(localCenter.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
        
        float penY = boxHalfExtents.y - std::abs(localCenter.y);
        if (penY < minPen) {
            minPen = penY;
            localNormal = glm::vec3(0.0f, localCenter.y > 0.0f ? 1.0f : -1.0f, 0.0f);
        }
        
        float penZ = boxHalfExtents.z - std::abs(localCenter.z);
        if (penZ < minPen) {
            minPen = penZ;
            localNormal = glm::vec3(0.0f, 0.0f, localCenter.z > 0.0f ? 1.0f : -1.0f);
        }
        
        contact.normal = boxRotation * localNormal;
        contact.penetration = minPen + sphereRadius;
        contact.point = sphereCenter - contact.normal * sphereRadius;
    }
    
    return true;
}

// ============================================================================
// AABB vs AABB
// ============================================================================
bool CollisionDetection::aabbAABB(const AABB& a, const AABB& b, Contact& contact) {
    if (!a.intersects(b)) {
        return false;
    }
    
    glm::vec3 centerA = a.getCenter();
    glm::vec3 centerB = b.getCenter();
    glm::vec3 extentsA = a.getExtents();
    glm::vec3 extentsB = b.getExtents();
    
    glm::vec3 d = centerB - centerA;
    glm::vec3 overlap = (extentsA + extentsB) - glm::abs(d);
    
    // Find axis of minimum penetration
    if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
        contact.normal = glm::vec3(d.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
        contact.penetration = overlap.x;
    } else if (overlap.y <= overlap.z) {
        contact.normal = glm::vec3(0.0f, d.y > 0.0f ? 1.0f : -1.0f, 0.0f);
        contact.penetration = overlap.y;
    } else {
        contact.normal = glm::vec3(0.0f, 0.0f, d.z > 0.0f ? 1.0f : -1.0f);
        contact.penetration = overlap.z;
    }
    
    // Contact point at center of overlap region
    AABB overlapAABB(glm::max(a.min, b.min), glm::min(a.max, b.max));
    contact.point = overlapAABB.getCenter();
    
    return true;
}

// ============================================================================
// SAT Helpers for Box-Box
// ============================================================================
float CollisionDetection::projectOnAxis(
    const glm::vec3& axis,
    const glm::vec3& halfExtents, const glm::mat3& rotation) {
    
    return halfExtents.x * std::abs(glm::dot(axis, rotation[0])) +
           halfExtents.y * std::abs(glm::dot(axis, rotation[1])) +
           halfExtents.z * std::abs(glm::dot(axis, rotation[2]));
}

bool CollisionDetection::overlapOnAxis(
    const glm::vec3& axis,
    const glm::vec3& centerA, const glm::vec3& halfExtentsA, const glm::mat3& rotationA,
    const glm::vec3& centerB, const glm::vec3& halfExtentsB, const glm::mat3& rotationB,
    float& overlap) {
    
    float projA = projectOnAxis(axis, halfExtentsA, rotationA);
    float projB = projectOnAxis(axis, halfExtentsB, rotationB);
    float dist = std::abs(glm::dot(centerB - centerA, axis));
    
    overlap = projA + projB - dist;
    return overlap > 0.0f;
}

// ============================================================================
// Box vs Box (OBB using SAT)
// ============================================================================
bool CollisionDetection::boxBox(
    const glm::vec3& centerA, const glm::vec3& halfExtentsA, const glm::mat3& rotationA,
    const glm::vec3& centerB, const glm::vec3& halfExtentsB, const glm::mat3& rotationB,
    std::vector<Contact>& contacts) {
    
    float minOverlap = std::numeric_limits<float>::max();
    glm::vec3 minAxis;
    
    // Test 15 axes: 3 from A, 3 from B, 9 cross products
    std::vector<glm::vec3> axes;
    
    // Face normals from A
    for (int i = 0; i < 3; i++) {
        axes.push_back(rotationA[i]);
    }
    
    // Face normals from B
    for (int i = 0; i < 3; i++) {
        axes.push_back(rotationB[i]);
    }
    
    // Cross products
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            glm::vec3 cross = glm::cross(rotationA[i], rotationB[j]);
            if (glm::dot(cross, cross) > 1e-6f) {
                axes.push_back(glm::normalize(cross));
            }
        }
    }
    
    for (const auto& axis : axes) {
        float overlap;
        if (!overlapOnAxis(axis, centerA, halfExtentsA, rotationA,
                           centerB, halfExtentsB, rotationB, overlap)) {
            return false; // Separating axis found
        }
        
        if (overlap < minOverlap) {
            minOverlap = overlap;
            minAxis = axis;
        }
    }
    
    // Ensure normal points from A to B
    if (glm::dot(minAxis, centerB - centerA) < 0.0f) {
        minAxis = -minAxis;
    }
    
    Contact contact;
    contact.normal = minAxis;
    contact.penetration = minOverlap;
    
    // Approximate contact point (center of overlap region)
    contact.point = (centerA + centerB) * 0.5f;
    
    contacts.push_back(contact);
    return true;
}

// ============================================================================
// Capsule vs Sphere
// ============================================================================
bool CollisionDetection::capsuleSphere(
    const glm::vec3& p0, const glm::vec3& p1, float capsuleRadius,
    const glm::vec3& sphereCenter, float sphereRadius,
    Contact& contact) {
    
    glm::vec3 closest = closestPointOnSegment(sphereCenter, p0, p1);
    return sphereSphere(closest, capsuleRadius, sphereCenter, sphereRadius, contact);
}

// ============================================================================
// Capsule vs Capsule
// ============================================================================
bool CollisionDetection::capsuleCapsule(
    const glm::vec3& p0A, const glm::vec3& p1A, float radiusA,
    const glm::vec3& p0B, const glm::vec3& p1B, float radiusB,
    Contact& contact) {
    
    glm::vec3 closestA, closestB;
    closestPointsBetweenSegments(p0A, p1A, p0B, p1B, closestA, closestB);
    
    return sphereSphere(closestA, radiusA, closestB, radiusB, contact);
}

// ============================================================================
// Capsule vs Box
// ============================================================================
bool CollisionDetection::capsuleBox(
    const glm::vec3& p0, const glm::vec3& p1, float capsuleRadius,
    const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
    const glm::mat3& boxRotation,
    std::vector<Contact>& contacts) {
    
    // Transform capsule to box local space
    glm::mat3 invRotation = glm::transpose(boxRotation);
    glm::vec3 localP0 = invRotation * (p0 - boxCenter);
    glm::vec3 localP1 = invRotation * (p1 - boxCenter);
    
    // Find closest point on capsule segment to box
    // Sample multiple points along the capsule
    const int numSamples = 4;
    bool anyContact = false;
    
    for (int i = 0; i < numSamples; i++) {
        float t = static_cast<float>(i) / static_cast<float>(numSamples - 1);
        glm::vec3 localPoint = localP0 + (localP1 - localP0) * t;
        glm::vec3 worldPoint = boxCenter + boxRotation * localPoint;
        
        Contact contact;
        if (sphereBox(worldPoint, capsuleRadius, boxCenter, boxHalfExtents, boxRotation, contact)) {
            contacts.push_back(contact);
            anyContact = true;
        }
    }
    
    return anyContact;
}

// ============================================================================
// Sphere vs Voxel
// ============================================================================
bool CollisionDetection::sphereVoxel(
    const glm::vec3& sphereCenter, float sphereRadius,
    const VoxelCollider& voxelCollider, const glm::mat4& voxelTransform,
    std::vector<Contact>& contacts) {
    
    // Transform sphere to voxel local space
    glm::mat4 invTransform = glm::inverse(voxelTransform);
    glm::vec3 localCenter = glm::vec3(invTransform * glm::vec4(sphereCenter, 1.0f));
    
    // Get AABB around sphere in local space
    AABB sphereAABB = AABB::fromCenterExtents(localCenter, glm::vec3(sphereRadius));
    
    // Get all potentially colliding blocks
    auto blocks = voxelCollider.getCollidingBlocks(sphereAABB);
    
    bool anyContact = false;
    
    for (const auto& blockAABB : blocks) {
        // Transform block AABB to world space
        AABB worldBlockAABB = blockAABB.transformed(voxelTransform);
        
        Contact contact;
        if (CollisionDetection::sphereAABB(sphereCenter, sphereRadius, worldBlockAABB, contact)) {
            contacts.push_back(contact);
            anyContact = true;
        }
    }
    
    return anyContact;
}

// ============================================================================
// Box vs Voxel
// ============================================================================
bool CollisionDetection::boxVoxel(
    const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
    const glm::mat3& boxRotation,
    const VoxelCollider& voxelCollider, const glm::mat4& voxelTransform,
    std::vector<Contact>& contacts) {
    
    // Get world-space AABB of the oriented box (conservative)
    glm::vec3 worldExtents(
        boxHalfExtents.x * std::abs(boxRotation[0].x) + 
        boxHalfExtents.y * std::abs(boxRotation[1].x) + 
        boxHalfExtents.z * std::abs(boxRotation[2].x),
        boxHalfExtents.x * std::abs(boxRotation[0].y) + 
        boxHalfExtents.y * std::abs(boxRotation[1].y) + 
        boxHalfExtents.z * std::abs(boxRotation[2].y),
        boxHalfExtents.x * std::abs(boxRotation[0].z) + 
        boxHalfExtents.y * std::abs(boxRotation[1].z) + 
        boxHalfExtents.z * std::abs(boxRotation[2].z)
    );
    
    AABB boxWorldAABB = AABB::fromCenterExtents(boxCenter, worldExtents);
    
    // Transform to voxel local space
    glm::mat4 invTransform = glm::inverse(voxelTransform);
    AABB localAABB = boxWorldAABB.transformed(invTransform);
    
    // Get all potentially colliding blocks
    auto blocks = voxelCollider.getCollidingBlocks(localAABB);
    
    bool anyContact = false;
    
    for (const auto& blockAABB : blocks) {
        // Transform block to world space
        AABB worldBlockAABB = blockAABB.transformed(voxelTransform);
        
        // For now, use AABB-AABB test (could use OBB-AABB for more accuracy)
        Contact contact;
        if (aabbAABB(boxWorldAABB, worldBlockAABB, contact)) {
            contacts.push_back(contact);
            anyContact = true;
        }
    }
    
    return anyContact;
}

// ============================================================================
// Capsule vs Voxel
// ============================================================================
bool CollisionDetection::capsuleVoxel(
    const glm::vec3& p0, const glm::vec3& p1, float capsuleRadius,
    const VoxelCollider& voxelCollider, const glm::mat4& voxelTransform,
    std::vector<Contact>& contacts) {
    
    // Get capsule AABB
    glm::vec3 minP = glm::min(p0, p1) - glm::vec3(capsuleRadius);
    glm::vec3 maxP = glm::max(p0, p1) + glm::vec3(capsuleRadius);
    AABB capsuleAABB(minP, maxP);
    
    // Transform to voxel local space
    glm::mat4 invTransform = glm::inverse(voxelTransform);
    AABB localAABB = capsuleAABB.transformed(invTransform);
    
    auto blocks = voxelCollider.getCollidingBlocks(localAABB);
    
    bool anyContact = false;
    
    for (const auto& blockAABB : blocks) {
        AABB worldBlockAABB = blockAABB.transformed(voxelTransform);
        
        // Test capsule against block AABB
        // Find closest point on capsule to block center
        glm::vec3 blockCenter = worldBlockAABB.getCenter();
        glm::vec3 closestOnCapsule = closestPointOnSegment(blockCenter, p0, p1);
        
        Contact contact;
        if (sphereAABB(closestOnCapsule, capsuleRadius, worldBlockAABB, contact)) {
            contacts.push_back(contact);
            anyContact = true;
        }
    }
    
    return anyContact;
}

// ============================================================================
// Generic Collider vs Collider
// ============================================================================
bool CollisionDetection::collide(
    const Collider& colliderA, const glm::mat4& transformA,
    const Collider& colliderB, const glm::mat4& transformB,
    std::vector<Contact>& contacts) {
    
    // Extract positions and rotations
    glm::vec3 posA = glm::vec3(transformA[3]);
    glm::vec3 posB = glm::vec3(transformB[3]);
    glm::mat3 rotA = glm::mat3(transformA);
    glm::mat3 rotB = glm::mat3(transformB);
    
    Collider::Type typeA = colliderA.getType();
    Collider::Type typeB = colliderB.getType();
    
    // Sphere vs Sphere
    if (typeA == Collider::Type::Sphere && typeB == Collider::Type::Sphere) {
        const auto& sphereA = static_cast<const SphereCollider&>(colliderA);
        const auto& sphereB = static_cast<const SphereCollider&>(colliderB);
        
        Contact contact;
        if (sphereSphere(posA + sphereA.getOffset(), sphereA.getRadius(),
                         posB + sphereB.getOffset(), sphereB.getRadius(), contact)) {
            contacts.push_back(contact);
            return true;
        }
    }
    
    // Sphere vs Box
    else if (typeA == Collider::Type::Sphere && typeB == Collider::Type::Box) {
        const auto& sphere = static_cast<const SphereCollider&>(colliderA);
        const auto& box = static_cast<const BoxCollider&>(colliderB);
        
        Contact contact;
        if (sphereBox(posA + sphere.getOffset(), sphere.getRadius(),
                      posB + box.getOffset(), box.getHalfExtents(), rotB, contact)) {
            contacts.push_back(contact);
            return true;
        }
    }
    else if (typeA == Collider::Type::Box && typeB == Collider::Type::Sphere) {
        const auto& box = static_cast<const BoxCollider&>(colliderA);
        const auto& sphere = static_cast<const SphereCollider&>(colliderB);
        
        Contact contact;
        if (sphereBox(posB + sphere.getOffset(), sphere.getRadius(),
                      posA + box.getOffset(), box.getHalfExtents(), rotA, contact)) {
            contact.normal = -contact.normal; // Flip normal
            contacts.push_back(contact);
            return true;
        }
    }
    
    // Box vs Box
    else if (typeA == Collider::Type::Box && typeB == Collider::Type::Box) {
        const auto& boxA = static_cast<const BoxCollider&>(colliderA);
        const auto& boxB = static_cast<const BoxCollider&>(colliderB);
        
        return boxBox(posA + boxA.getOffset(), boxA.getHalfExtents(), rotA,
                      posB + boxB.getOffset(), boxB.getHalfExtents(), rotB, contacts);
    }
    
    // Capsule cases
    else if (typeA == Collider::Type::Capsule || typeB == Collider::Type::Capsule) {
        // Get capsule endpoints
        auto getCapsuleEndpoints = [](const CapsuleCollider& capsule, 
                                       const glm::vec3& pos, const glm::mat3& rot,
                                       glm::vec3& p0, glm::vec3& p1) {
            float halfSegment = (capsule.getHeight() - 2.0f * capsule.getRadius()) * 0.5f;
            halfSegment = std::max(halfSegment, 0.0f);
            glm::vec3 up = rot * glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 center = pos + capsule.getOffset();
            p0 = center - up * halfSegment;
            p1 = center + up * halfSegment;
        };
        
        if (typeA == Collider::Type::Capsule && typeB == Collider::Type::Capsule) {
            const auto& capA = static_cast<const CapsuleCollider&>(colliderA);
            const auto& capB = static_cast<const CapsuleCollider&>(colliderB);
            
            glm::vec3 p0A, p1A, p0B, p1B;
            getCapsuleEndpoints(capA, posA, rotA, p0A, p1A);
            getCapsuleEndpoints(capB, posB, rotB, p0B, p1B);
            
            Contact contact;
            if (capsuleCapsule(p0A, p1A, capA.getRadius(), p0B, p1B, capB.getRadius(), contact)) {
                contacts.push_back(contact);
                return true;
            }
        }
        else if (typeA == Collider::Type::Capsule && typeB == Collider::Type::Sphere) {
            const auto& capsule = static_cast<const CapsuleCollider&>(colliderA);
            const auto& sphere = static_cast<const SphereCollider&>(colliderB);
            
            glm::vec3 p0, p1;
            getCapsuleEndpoints(capsule, posA, rotA, p0, p1);
            
            Contact contact;
            if (capsuleSphere(p0, p1, capsule.getRadius(),
                              posB + sphere.getOffset(), sphere.getRadius(), contact)) {
                contacts.push_back(contact);
                return true;
            }
        }
        else if (typeA == Collider::Type::Sphere && typeB == Collider::Type::Capsule) {
            const auto& sphere = static_cast<const SphereCollider&>(colliderA);
            const auto& capsule = static_cast<const CapsuleCollider&>(colliderB);
            
            glm::vec3 p0, p1;
            getCapsuleEndpoints(capsule, posB, rotB, p0, p1);
            
            Contact contact;
            if (capsuleSphere(p0, p1, capsule.getRadius(),
                              posA + sphere.getOffset(), sphere.getRadius(), contact)) {
                contact.normal = -contact.normal;
                contacts.push_back(contact);
                return true;
            }
        }
        else if (typeA == Collider::Type::Capsule && typeB == Collider::Type::Box) {
            const auto& capsule = static_cast<const CapsuleCollider&>(colliderA);
            const auto& box = static_cast<const BoxCollider&>(colliderB);
            
            glm::vec3 p0, p1;
            getCapsuleEndpoints(capsule, posA, rotA, p0, p1);
            
            return capsuleBox(p0, p1, capsule.getRadius(),
                              posB + box.getOffset(), box.getHalfExtents(), rotB, contacts);
        }
    }
    
    // Voxel cases
    else if (typeB == Collider::Type::Voxel) {
        const auto& voxel = static_cast<const VoxelCollider&>(colliderB);
        
        if (typeA == Collider::Type::Sphere) {
            const auto& sphere = static_cast<const SphereCollider&>(colliderA);
            return sphereVoxel(posA + sphere.getOffset(), sphere.getRadius(), voxel, transformB, contacts);
        }
        else if (typeA == Collider::Type::Box) {
            const auto& box = static_cast<const BoxCollider&>(colliderA);
            return boxVoxel(posA + box.getOffset(), box.getHalfExtents(), rotA, voxel, transformB, contacts);
        }
        else if (typeA == Collider::Type::Capsule) {
            const auto& capsule = static_cast<const CapsuleCollider&>(colliderA);
            float halfSegment = (capsule.getHeight() - 2.0f * capsule.getRadius()) * 0.5f;
            halfSegment = std::max(halfSegment, 0.0f);
            glm::vec3 up = rotA * glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 center = posA + capsule.getOffset();
            return capsuleVoxel(center - up * halfSegment, center + up * halfSegment, 
                                capsule.getRadius(), voxel, transformB, contacts);
        }
    }
    
    return false;
}

// ============================================================================
// Sweep Tests
// ============================================================================
SweepResult CollisionDetection::sweepSphereAABB(
    const glm::vec3& sphereStart, const glm::vec3& sphereEnd, float radius,
    const AABB& aabb) {
    
    SweepResult result;
    
    // Expand AABB by sphere radius
    AABB expandedAABB = aabb.expanded(radius);
    
    // Ray from start to end
    glm::vec3 direction = sphereEnd - sphereStart;
    float distance = glm::length(direction);
    
    if (distance < 1e-6f) {
        // No movement
        Contact contact;
        if (sphereAABB(sphereStart, radius, aabb, contact)) {
            result.hit = true;
            result.time = 0.0f;
            result.point = contact.point;
            result.normal = contact.normal;
        }
        return result;
    }
    
    direction /= distance;
    
    Ray ray(sphereStart, direction, distance);
    RaycastHit hit;
    
    if (rayAABB(ray, expandedAABB, hit)) {
        result.hit = true;
        result.time = hit.distance / distance;
        result.point = hit.point - hit.normal * radius;
        result.normal = hit.normal;
    }
    
    return result;
}

SweepResult CollisionDetection::sweepAABB(
    const AABB& movingAABB, const glm::vec3& velocity,
    const AABB& staticAABB) {
    
    SweepResult result;
    
    float distance = glm::length(velocity);
    if (distance < 1e-6f) {
        Contact contact;
        if (aabbAABB(movingAABB, staticAABB, contact)) {
            result.hit = true;
            result.time = 0.0f;
            result.point = contact.point;
            result.normal = contact.normal;
        }
        return result;
    }
    
    // Minkowski sum: expand static by moving's half-extents
    glm::vec3 extents = movingAABB.getExtents();
    AABB expandedStatic = staticAABB.expanded(0.0f);
    expandedStatic.min -= extents;
    expandedStatic.max += extents;
    
    // Ray from moving's center
    glm::vec3 center = movingAABB.getCenter();
    glm::vec3 direction = glm::normalize(velocity);
    
    Ray ray(center, direction, distance);
    RaycastHit hit;
    
    if (rayAABB(ray, expandedStatic, hit)) {
        result.hit = true;
        result.time = hit.distance / distance;
        result.point = hit.point;
        result.normal = hit.normal;
    }
    
    return result;
}

SweepResult CollisionDetection::sweepSphereVoxel(
    const glm::vec3& sphereStart, const glm::vec3& sphereEnd, float radius,
    const VoxelCollider& voxelCollider, const glm::mat4& voxelTransform) {
    
    SweepResult result;
    
    // Get path AABB
    glm::vec3 pathMin = glm::min(sphereStart, sphereEnd) - glm::vec3(radius);
    glm::vec3 pathMax = glm::max(sphereStart, sphereEnd) + glm::vec3(radius);
    AABB pathAABB(pathMin, pathMax);
    
    // Transform to voxel local space
    glm::mat4 invTransform = glm::inverse(voxelTransform);
    AABB localPathAABB = pathAABB.transformed(invTransform);
    
    // Get all blocks along path
    auto blocks = voxelCollider.getCollidingBlocks(localPathAABB);
    
    float minTime = 1.0f;
    
    for (const auto& blockAABB : blocks) {
        AABB worldBlockAABB = blockAABB.transformed(voxelTransform);
        
        SweepResult blockResult = sweepSphereAABB(sphereStart, sphereEnd, radius, worldBlockAABB);
        if (blockResult.hit && blockResult.time < minTime) {
            minTime = blockResult.time;
            result = blockResult;
        }
    }
    
    return result;
}

// ============================================================================
// Raycasting
// ============================================================================
bool CollisionDetection::raySphere(
    const Ray& ray,
    const glm::vec3& center, float radius,
    RaycastHit& hit) {
    
    SphereCollider sphere(radius);
    sphere.setOffset(center);
    return sphere.raycast(ray, hit);
}

bool CollisionDetection::rayAABB(const Ray& ray, const AABB& aabb, RaycastHit& hit) {
    float tMin = 0.0f;
    float tMax = ray.maxDistance;
    int hitAxis = -1;
    bool hitMinSide = false;
    
    for (int i = 0; i < 3; i++) {
        if (std::abs(ray.direction[i]) < 1e-8f) {
            // Ray parallel to slab
            if (ray.origin[i] < aabb.min[i] || ray.origin[i] > aabb.max[i]) {
                return false;
            }
        } else {
            float invD = 1.0f / ray.direction[i];
            float t0 = (aabb.min[i] - ray.origin[i]) * invD;
            float t1 = (aabb.max[i] - ray.origin[i]) * invD;
            
            bool swapped = false;
            if (invD < 0.0f) {
                std::swap(t0, t1);
                swapped = true;
            }
            
            if (t0 > tMin) {
                tMin = t0;
                hitAxis = i;
                hitMinSide = !swapped;
            }
            
            tMax = std::min(tMax, t1);
            
            if (tMax < tMin) {
                return false;
            }
        }
    }
    
    if (tMin < 0.0f) {
        return false;
    }
    
    hit.hit = true;
    hit.distance = tMin;
    hit.point = ray.getPoint(tMin);
    hit.normal = glm::vec3(0.0f);
    
    if (hitAxis >= 0) {
        hit.normal[hitAxis] = hitMinSide ? -1.0f : 1.0f;
    }
    
    return true;
}

bool CollisionDetection::rayBox(
    const Ray& ray,
    const glm::vec3& center, const glm::vec3& halfExtents,
    const glm::mat3& rotation,
    RaycastHit& hit) {
    
    // Transform ray to box local space
    glm::mat3 invRotation = glm::transpose(rotation);
    glm::vec3 localOrigin = invRotation * (ray.origin - center);
    glm::vec3 localDirection = invRotation * ray.direction;
    
    Ray localRay(localOrigin, localDirection, ray.maxDistance);
    AABB localAABB(-halfExtents, halfExtents);
    
    if (rayAABB(localRay, localAABB, hit)) {
        // Transform hit back to world space
        hit.point = center + rotation * hit.point;
        hit.normal = rotation * hit.normal;
        return true;
    }
    
    return false;
}

bool CollisionDetection::rayCapsule(
    const Ray& ray,
    const glm::vec3& p0, const glm::vec3& p1, float radius,
    RaycastHit& hit) {
    
    CapsuleCollider capsule(radius, glm::length(p1 - p0) + 2.0f * radius);
    
    // Create local space where capsule is aligned with Y axis
    glm::vec3 center = (p0 + p1) * 0.5f;
    glm::vec3 up = glm::normalize(p1 - p0);
    
    // Transform ray to capsule local space
    // For simplicity, use the capsule's built-in raycast
    Ray localRay;
    localRay.origin = ray.origin - center;
    localRay.direction = ray.direction;
    localRay.maxDistance = ray.maxDistance;
    
    capsule.setOffset(glm::vec3(0.0f));
    
    if (capsule.raycast(localRay, hit)) {
        hit.point += center;
        return true;
    }
    
    return false;
}

} // namespace Physics
