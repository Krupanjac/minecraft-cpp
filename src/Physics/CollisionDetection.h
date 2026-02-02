#pragma once

#include "PhysicsTypes.h"
#include "Collider.h"
#include <vector>

namespace Physics {

// ============================================================================
// Collision Detection - Narrow phase algorithms
// ============================================================================
class CollisionDetection {
public:
    // ========== Primitive Tests ==========
    
    // Sphere vs Sphere
    static bool sphereSphere(
        const glm::vec3& centerA, float radiusA,
        const glm::vec3& centerB, float radiusB,
        Contact& contact);
    
    // Sphere vs Box (OBB)
    static bool sphereBox(
        const glm::vec3& sphereCenter, float sphereRadius,
        const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
        const glm::mat3& boxRotation,
        Contact& contact);
    
    // Sphere vs AABB
    static bool sphereAABB(
        const glm::vec3& sphereCenter, float sphereRadius,
        const AABB& aabb,
        Contact& contact);
    
    // Box vs Box (OBB using SAT)
    static bool boxBox(
        const glm::vec3& centerA, const glm::vec3& halfExtentsA, const glm::mat3& rotationA,
        const glm::vec3& centerB, const glm::vec3& halfExtentsB, const glm::mat3& rotationB,
        std::vector<Contact>& contacts);
    
    // AABB vs AABB
    static bool aabbAABB(
        const AABB& a, const AABB& b,
        Contact& contact);
    
    // Capsule vs Capsule
    static bool capsuleCapsule(
        const glm::vec3& p0A, const glm::vec3& p1A, float radiusA,
        const glm::vec3& p0B, const glm::vec3& p1B, float radiusB,
        Contact& contact);
    
    // Capsule vs Sphere
    static bool capsuleSphere(
        const glm::vec3& p0, const glm::vec3& p1, float capsuleRadius,
        const glm::vec3& sphereCenter, float sphereRadius,
        Contact& contact);
    
    // Capsule vs Box
    static bool capsuleBox(
        const glm::vec3& p0, const glm::vec3& p1, float capsuleRadius,
        const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
        const glm::mat3& boxRotation,
        std::vector<Contact>& contacts);
    
    // ========== Collider-based Tests ==========
    
    // Generic collider vs collider (dispatches to specific functions)
    static bool collide(
        const Collider& colliderA, const glm::mat4& transformA,
        const Collider& colliderB, const glm::mat4& transformB,
        std::vector<Contact>& contacts);
    
    // ========== Voxel-specific Tests ==========
    
    // Sphere vs Voxel terrain
    static bool sphereVoxel(
        const glm::vec3& sphereCenter, float sphereRadius,
        const VoxelCollider& voxelCollider, const glm::mat4& voxelTransform,
        std::vector<Contact>& contacts);
    
    // Box vs Voxel terrain
    static bool boxVoxel(
        const glm::vec3& boxCenter, const glm::vec3& boxHalfExtents,
        const glm::mat3& boxRotation,
        const VoxelCollider& voxelCollider, const glm::mat4& voxelTransform,
        std::vector<Contact>& contacts);
    
    // Capsule vs Voxel terrain
    static bool capsuleVoxel(
        const glm::vec3& p0, const glm::vec3& p1, float capsuleRadius,
        const VoxelCollider& voxelCollider, const glm::mat4& voxelTransform,
        std::vector<Contact>& contacts);
    
    // ========== Continuous Collision Detection ==========
    
    // Sphere sweep vs AABB (for fast-moving objects)
    static SweepResult sweepSphereAABB(
        const glm::vec3& sphereStart, const glm::vec3& sphereEnd, float radius,
        const AABB& aabb);
    
    // AABB sweep vs AABB
    static SweepResult sweepAABB(
        const AABB& movingAABB, const glm::vec3& velocity,
        const AABB& staticAABB);
    
    // Sphere sweep vs voxel terrain
    static SweepResult sweepSphereVoxel(
        const glm::vec3& sphereStart, const glm::vec3& sphereEnd, float radius,
        const VoxelCollider& voxelCollider, const glm::mat4& voxelTransform);
    
    // ========== Raycasting ==========
    
    // Ray vs Sphere
    static bool raySphere(
        const Ray& ray,
        const glm::vec3& center, float radius,
        RaycastHit& hit);
    
    // Ray vs AABB
    static bool rayAABB(
        const Ray& ray,
        const AABB& aabb,
        RaycastHit& hit);
    
    // Ray vs OBB
    static bool rayBox(
        const Ray& ray,
        const glm::vec3& center, const glm::vec3& halfExtents,
        const glm::mat3& rotation,
        RaycastHit& hit);
    
    // Ray vs Capsule
    static bool rayCapsule(
        const Ray& ray,
        const glm::vec3& p0, const glm::vec3& p1, float radius,
        RaycastHit& hit);
    
    // ========== Utility Functions ==========
    
    // Closest point on line segment to point
    static glm::vec3 closestPointOnSegment(
        const glm::vec3& point,
        const glm::vec3& segStart, const glm::vec3& segEnd);
    
    // Closest points between two line segments
    static void closestPointsBetweenSegments(
        const glm::vec3& p0A, const glm::vec3& p1A,
        const glm::vec3& p0B, const glm::vec3& p1B,
        glm::vec3& closestA, glm::vec3& closestB);
    
    // Compute contact normal for AABB penetration
    static glm::vec3 computeAABBContactNormal(
        const AABB& a, const AABB& b);
    
private:
    // SAT helpers for Box-Box collision
    static float projectOnAxis(
        const glm::vec3& axis,
        const glm::vec3& halfExtents, const glm::mat3& rotation);
    
    static bool overlapOnAxis(
        const glm::vec3& axis,
        const glm::vec3& centerA, const glm::vec3& halfExtentsA, const glm::mat3& rotationA,
        const glm::vec3& centerB, const glm::vec3& halfExtentsB, const glm::mat3& rotationB,
        float& overlap);
};

} // namespace Physics
