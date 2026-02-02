#include "Collider.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Physics {

// ============================================================================
// AABB Implementation (from PhysicsTypes.h)
// ============================================================================
AABB AABB::transformed(const glm::mat4& transform) const {
    // Transform all 8 corners and compute new AABB
    glm::vec3 corners[8] = {
        glm::vec3(min.x, min.y, min.z),
        glm::vec3(max.x, min.y, min.z),
        glm::vec3(min.x, max.y, min.z),
        glm::vec3(max.x, max.y, min.z),
        glm::vec3(min.x, min.y, max.z),
        glm::vec3(max.x, min.y, max.z),
        glm::vec3(min.x, max.y, max.z),
        glm::vec3(max.x, max.y, max.z)
    };
    
    glm::vec3 newMin(std::numeric_limits<float>::max());
    glm::vec3 newMax(std::numeric_limits<float>::lowest());
    
    for (int i = 0; i < 8; i++) {
        glm::vec4 transformed = transform * glm::vec4(corners[i], 1.0f);
        glm::vec3 point = glm::vec3(transformed) / transformed.w;
        newMin = glm::min(newMin, point);
        newMax = glm::max(newMax, point);
    }
    
    return AABB(newMin, newMax);
}

// ============================================================================
// Collider Base Class
// ============================================================================
AABB Collider::getWorldAABB(const glm::mat4& transform) const {
    AABB localAABB = getLocalAABB();
    // Apply offset
    localAABB.min += offset;
    localAABB.max += offset;
    return localAABB.transformed(transform);
}

// ============================================================================
// Sphere Collider
// ============================================================================
SphereCollider::SphereCollider(float radius)
    : Collider(Type::Sphere)
    , radius(radius)
{
}

AABB SphereCollider::getLocalAABB() const {
    return AABB::fromCenterExtents(offset, glm::vec3(radius));
}

bool SphereCollider::containsPoint(const glm::vec3& localPoint) const {
    glm::vec3 relPoint = localPoint - offset;
    return glm::dot(relPoint, relPoint) <= radius * radius;
}

glm::vec3 SphereCollider::getClosestPoint(const glm::vec3& localPoint) const {
    glm::vec3 relPoint = localPoint - offset;
    float distSq = glm::dot(relPoint, relPoint);
    
    if (distSq <= radius * radius) {
        return localPoint; // Point is inside
    }
    
    return offset + glm::normalize(relPoint) * radius;
}

bool SphereCollider::raycast(const Ray& localRay, RaycastHit& hit) const {
    glm::vec3 oc = localRay.origin - offset;
    
    float a = glm::dot(localRay.direction, localRay.direction);
    float b = 2.0f * glm::dot(oc, localRay.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;
    
    if (discriminant < 0.0f) {
        return false;
    }
    
    float sqrtD = std::sqrt(discriminant);
    float t = (-b - sqrtD) / (2.0f * a);
    
    if (t < 0.0f) {
        t = (-b + sqrtD) / (2.0f * a);
    }
    
    if (t < 0.0f || t > localRay.maxDistance) {
        return false;
    }
    
    hit.hit = true;
    hit.distance = t;
    hit.point = localRay.getPoint(t);
    hit.normal = glm::normalize(hit.point - offset);
    
    return true;
}

glm::vec3 SphereCollider::support(const glm::vec3& direction) const {
    return offset + glm::normalize(direction) * radius;
}

// ============================================================================
// Box Collider
// ============================================================================
BoxCollider::BoxCollider(const glm::vec3& halfExtents)
    : Collider(Type::Box)
    , halfExtents(halfExtents)
{
}

AABB BoxCollider::getLocalAABB() const {
    return AABB(offset - halfExtents, offset + halfExtents);
}

bool BoxCollider::containsPoint(const glm::vec3& localPoint) const {
    glm::vec3 relPoint = localPoint - offset;
    return std::abs(relPoint.x) <= halfExtents.x &&
           std::abs(relPoint.y) <= halfExtents.y &&
           std::abs(relPoint.z) <= halfExtents.z;
}

glm::vec3 BoxCollider::getClosestPoint(const glm::vec3& localPoint) const {
    glm::vec3 relPoint = localPoint - offset;
    glm::vec3 closest;
    
    closest.x = std::clamp(relPoint.x, -halfExtents.x, halfExtents.x);
    closest.y = std::clamp(relPoint.y, -halfExtents.y, halfExtents.y);
    closest.z = std::clamp(relPoint.z, -halfExtents.z, halfExtents.z);
    
    return closest + offset;
}

bool BoxCollider::raycast(const Ray& localRay, RaycastHit& hit) const {
    glm::vec3 minB = offset - halfExtents;
    glm::vec3 maxB = offset + halfExtents;
    
    float tMin = 0.0f;
    float tMax = localRay.maxDistance;
    int hitAxis = -1;
    bool hitMinSide = false;
    
    for (int i = 0; i < 3; i++) {
        float invD = 1.0f / localRay.direction[i];
        float t0 = (minB[i] - localRay.origin[i]) * invD;
        float t1 = (maxB[i] - localRay.origin[i]) * invD;
        
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
    
    if (tMin < 0.0f) {
        return false;
    }
    
    hit.hit = true;
    hit.distance = tMin;
    hit.point = localRay.getPoint(tMin);
    hit.normal = glm::vec3(0.0f);
    
    if (hitAxis >= 0) {
        hit.normal[hitAxis] = hitMinSide ? -1.0f : 1.0f;
    }
    
    return true;
}

glm::vec3 BoxCollider::support(const glm::vec3& direction) const {
    return offset + glm::vec3(
        direction.x >= 0.0f ? halfExtents.x : -halfExtents.x,
        direction.y >= 0.0f ? halfExtents.y : -halfExtents.y,
        direction.z >= 0.0f ? halfExtents.z : -halfExtents.z
    );
}

// ============================================================================
// Capsule Collider
// ============================================================================
CapsuleCollider::CapsuleCollider(float radius, float height)
    : Collider(Type::Capsule)
    , radius(radius)
    , height(height)
{
}

void CapsuleCollider::getSegmentEndpoints(glm::vec3& p0, glm::vec3& p1) const {
    float halfSegment = (height - 2.0f * radius) * 0.5f;
    halfSegment = std::max(halfSegment, 0.0f);
    p0 = offset + glm::vec3(0.0f, -halfSegment, 0.0f);
    p1 = offset + glm::vec3(0.0f, halfSegment, 0.0f);
}

AABB CapsuleCollider::getLocalAABB() const {
    float halfHeight = height * 0.5f;
    return AABB(
        offset - glm::vec3(radius, halfHeight, radius),
        offset + glm::vec3(radius, halfHeight, radius)
    );
}

bool CapsuleCollider::containsPoint(const glm::vec3& localPoint) const {
    glm::vec3 p0, p1;
    getSegmentEndpoints(p0, p1);
    
    // Find closest point on line segment
    glm::vec3 segment = p1 - p0;
    float segLenSq = glm::dot(segment, segment);
    
    float t = 0.0f;
    if (segLenSq > 1e-6f) {
        t = std::clamp(glm::dot(localPoint - p0, segment) / segLenSq, 0.0f, 1.0f);
    }
    
    glm::vec3 closestOnSegment = p0 + segment * t;
    float distSq = glm::dot(localPoint - closestOnSegment, localPoint - closestOnSegment);
    
    return distSq <= radius * radius;
}

glm::vec3 CapsuleCollider::getClosestPoint(const glm::vec3& localPoint) const {
    glm::vec3 p0, p1;
    getSegmentEndpoints(p0, p1);
    
    glm::vec3 segment = p1 - p0;
    float segLenSq = glm::dot(segment, segment);
    
    float t = 0.0f;
    if (segLenSq > 1e-6f) {
        t = std::clamp(glm::dot(localPoint - p0, segment) / segLenSq, 0.0f, 1.0f);
    }
    
    glm::vec3 closestOnSegment = p0 + segment * t;
    glm::vec3 toPoint = localPoint - closestOnSegment;
    float dist = glm::length(toPoint);
    
    if (dist <= radius) {
        return localPoint; // Inside capsule
    }
    
    return closestOnSegment + (toPoint / dist) * radius;
}

bool CapsuleCollider::raycast(const Ray& localRay, RaycastHit& hit) const {
    glm::vec3 p0, p1;
    getSegmentEndpoints(p0, p1);
    
    // Capsule = infinite cylinder + two spheres at endpoints
    // Test cylinder first
    glm::vec3 d = p1 - p0;
    glm::vec3 m = localRay.origin - p0;
    
    float dd = glm::dot(d, d);
    float nd = glm::dot(localRay.direction, d);
    float md = glm::dot(m, d);
    
    float a = dd - nd * nd;
    float c = dd * glm::dot(m, m) - md * md - radius * radius * dd;
    
    if (std::abs(a) < 1e-6f) {
        // Ray parallel to cylinder axis - test spheres only
    } else {
        float b = dd * glm::dot(m, localRay.direction) - nd * md;
        float discriminant = b * b - a * c;
        
        if (discriminant >= 0.0f) {
            float t = (-b - std::sqrt(discriminant)) / a;
            
            if (t >= 0.0f && t <= localRay.maxDistance) {
                float y = md + t * nd;
                
                // Check if hit is within cylinder segment
                if (y >= 0.0f && y <= dd) {
                    hit.hit = true;
                    hit.distance = t;
                    hit.point = localRay.getPoint(t);
                    
                    // Normal on cylinder surface
                    glm::vec3 axisPoint = p0 + d * (y / dd);
                    hit.normal = glm::normalize(hit.point - axisPoint);
                    return true;
                }
            }
        }
    }
    
    // Test sphere caps
    SphereCollider sphere0(radius);
    sphere0.setOffset(p0);
    SphereCollider sphere1(radius);
    sphere1.setOffset(p1);
    
    RaycastHit hit0, hit1;
    bool h0 = sphere0.raycast(localRay, hit0);
    bool h1 = sphere1.raycast(localRay, hit1);
    
    if (h0 && h1) {
        hit = hit0.distance < hit1.distance ? hit0 : hit1;
        return true;
    } else if (h0) {
        hit = hit0;
        return true;
    } else if (h1) {
        hit = hit1;
        return true;
    }
    
    return false;
}

glm::vec3 CapsuleCollider::support(const glm::vec3& direction) const {
    glm::vec3 p0, p1;
    getSegmentEndpoints(p0, p1);
    
    // Pick the endpoint furthest in direction
    glm::vec3 base = (glm::dot(direction, p1) > glm::dot(direction, p0)) ? p1 : p0;
    
    // Add radius in direction
    return base + glm::normalize(direction) * radius;
}

// ============================================================================
// Compound Collider
// ============================================================================
CompoundCollider::CompoundCollider()
    : Collider(Type::Mesh)
{
}

void CompoundCollider::addCollider(std::shared_ptr<Collider> collider, 
                                    const glm::vec3& position,
                                    const glm::quat& rotation) {
    children.push_back({collider, position, rotation});
    aabbDirty = true;
}

void CompoundCollider::removeCollider(size_t index) {
    if (index < children.size()) {
        children.erase(children.begin() + index);
        aabbDirty = true;
    }
}

void CompoundCollider::clearColliders() {
    children.clear();
    aabbDirty = true;
}

AABB CompoundCollider::getLocalAABB() const {
    if (children.empty()) {
        return AABB(offset, offset);
    }
    
    if (aabbDirty) {
        cachedAABB = AABB(
            glm::vec3(std::numeric_limits<float>::max()),
            glm::vec3(std::numeric_limits<float>::lowest())
        );
        
        for (const auto& child : children) {
            glm::mat4 childTransform = glm::translate(glm::mat4(1.0f), child.position);
            childTransform *= glm::mat4_cast(child.rotation);
            
            AABB childAABB = child.collider->getWorldAABB(childTransform);
            cachedAABB = cachedAABB.merged(childAABB);
        }
        
        cachedAABB.min += offset;
        cachedAABB.max += offset;
        aabbDirty = false;
    }
    
    return cachedAABB;
}

bool CompoundCollider::containsPoint(const glm::vec3& localPoint) const {
    for (const auto& child : children) {
        // Transform point to child's local space
        glm::mat4 childTransform = glm::translate(glm::mat4(1.0f), child.position);
        childTransform *= glm::mat4_cast(child.rotation);
        glm::mat4 invTransform = glm::inverse(childTransform);
        
        glm::vec3 childLocalPoint = glm::vec3(invTransform * glm::vec4(localPoint - offset, 1.0f));
        
        if (child.collider->containsPoint(childLocalPoint)) {
            return true;
        }
    }
    return false;
}

glm::vec3 CompoundCollider::getClosestPoint(const glm::vec3& localPoint) const {
    if (children.empty()) {
        return localPoint;
    }
    
    glm::vec3 closest = localPoint;
    float minDistSq = std::numeric_limits<float>::max();
    
    for (const auto& child : children) {
        glm::mat4 childTransform = glm::translate(glm::mat4(1.0f), child.position);
        childTransform *= glm::mat4_cast(child.rotation);
        glm::mat4 invTransform = glm::inverse(childTransform);
        
        glm::vec3 childLocalPoint = glm::vec3(invTransform * glm::vec4(localPoint - offset, 1.0f));
        glm::vec3 childClosest = child.collider->getClosestPoint(childLocalPoint);
        
        // Transform back to compound local space
        glm::vec3 worldClosest = glm::vec3(childTransform * glm::vec4(childClosest, 1.0f)) + offset;
        
        float distSq = glm::dot(worldClosest - localPoint, worldClosest - localPoint);
        if (distSq < minDistSq) {
            minDistSq = distSq;
            closest = worldClosest;
        }
    }
    
    return closest;
}

bool CompoundCollider::raycast(const Ray& localRay, RaycastHit& hit) const {
    bool anyHit = false;
    hit.distance = localRay.maxDistance;
    
    Ray adjustedRay = localRay;
    adjustedRay.origin -= offset;
    
    for (const auto& child : children) {
        glm::mat4 childTransform = glm::translate(glm::mat4(1.0f), child.position);
        childTransform *= glm::mat4_cast(child.rotation);
        glm::mat4 invTransform = glm::inverse(childTransform);
        
        // Transform ray to child's local space
        Ray childRay;
        childRay.origin = glm::vec3(invTransform * glm::vec4(adjustedRay.origin, 1.0f));
        childRay.direction = glm::normalize(glm::vec3(invTransform * glm::vec4(adjustedRay.direction, 0.0f)));
        childRay.maxDistance = hit.distance;
        
        RaycastHit childHit;
        if (child.collider->raycast(childRay, childHit)) {
            if (childHit.distance < hit.distance) {
                hit = childHit;
                // Transform hit back to compound local space
                hit.point = glm::vec3(childTransform * glm::vec4(childHit.point, 1.0f)) + offset;
                hit.normal = glm::normalize(glm::vec3(childTransform * glm::vec4(childHit.normal, 0.0f)));
                anyHit = true;
            }
        }
    }
    
    return anyHit;
}

glm::vec3 CompoundCollider::support(const glm::vec3& direction) const {
    if (children.empty()) {
        return offset;
    }
    
    glm::vec3 best = offset;
    float maxDot = std::numeric_limits<float>::lowest();
    
    for (const auto& child : children) {
        glm::mat4 childTransform = glm::translate(glm::mat4(1.0f), child.position);
        childTransform *= glm::mat4_cast(child.rotation);
        glm::mat4 invTransform = glm::inverse(childTransform);
        
        // Transform direction to child's local space
        glm::vec3 childDir = glm::normalize(glm::vec3(invTransform * glm::vec4(direction, 0.0f)));
        glm::vec3 childSupport = child.collider->support(childDir);
        
        // Transform back
        glm::vec3 worldSupport = glm::vec3(childTransform * glm::vec4(childSupport, 1.0f)) + offset;
        
        float d = glm::dot(worldSupport, direction);
        if (d > maxDot) {
            maxDot = d;
            best = worldSupport;
        }
    }
    
    return best;
}

// ============================================================================
// Voxel Collider
// ============================================================================
VoxelCollider::VoxelCollider(const glm::ivec3& minBlock, const glm::ivec3& maxBlock, BlockCheckFunc checkFunc)
    : Collider(Type::Voxel)
    , minBlock(minBlock)
    , maxBlock(maxBlock)
    , blockCheck(checkFunc)
{
}

AABB VoxelCollider::getLocalAABB() const {
    return AABB(
        glm::vec3(minBlock) + offset,
        glm::vec3(maxBlock + glm::ivec3(1)) + offset
    );
}

bool VoxelCollider::containsPoint(const glm::vec3& localPoint) const {
    glm::vec3 adjustedPoint = localPoint - offset;
    glm::ivec3 blockPos = glm::ivec3(glm::floor(adjustedPoint));
    
    if (blockPos.x < minBlock.x || blockPos.x > maxBlock.x ||
        blockPos.y < minBlock.y || blockPos.y > maxBlock.y ||
        blockPos.z < minBlock.z || blockPos.z > maxBlock.z) {
        return false;
    }
    
    return isBlockSolid(blockPos.x, blockPos.y, blockPos.z);
}

glm::vec3 VoxelCollider::getClosestPoint(const glm::vec3& localPoint) const {
    // For voxel terrain, return the point itself if inside solid, 
    // otherwise return the query point
    if (containsPoint(localPoint)) {
        // Find nearest surface
        glm::vec3 adjustedPoint = localPoint - offset;
        glm::ivec3 blockPos = glm::ivec3(glm::floor(adjustedPoint));
        
        // Get fractional position within block
        glm::vec3 frac = adjustedPoint - glm::vec3(blockPos);
        
        // Find closest face
        float minDist = std::min({frac.x, 1.0f - frac.x, 
                                   frac.y, 1.0f - frac.y, 
                                   frac.z, 1.0f - frac.z});
        
        glm::vec3 result = adjustedPoint;
        if (minDist == frac.x) result.x = static_cast<float>(blockPos.x);
        else if (minDist == 1.0f - frac.x) result.x = static_cast<float>(blockPos.x + 1);
        else if (minDist == frac.y) result.y = static_cast<float>(blockPos.y);
        else if (minDist == 1.0f - frac.y) result.y = static_cast<float>(blockPos.y + 1);
        else if (minDist == frac.z) result.z = static_cast<float>(blockPos.z);
        else result.z = static_cast<float>(blockPos.z + 1);
        
        return result + offset;
    }
    
    return localPoint;
}

bool VoxelCollider::raycast(const Ray& localRay, RaycastHit& hit) const {
    // DDA voxel traversal algorithm
    glm::vec3 origin = localRay.origin - offset;
    glm::vec3 direction = localRay.direction;
    
    glm::ivec3 pos = glm::ivec3(glm::floor(origin));
    glm::ivec3 step = glm::ivec3(
        direction.x >= 0 ? 1 : -1,
        direction.y >= 0 ? 1 : -1,
        direction.z >= 0 ? 1 : -1
    );
    
    glm::vec3 tDelta = glm::abs(glm::vec3(1.0f) / direction);
    
    glm::vec3 tMax;
    for (int i = 0; i < 3; i++) {
        if (std::abs(direction[i]) < 1e-6f) {
            tMax[i] = std::numeric_limits<float>::max();
        } else if (step[i] > 0) {
            tMax[i] = (static_cast<float>(pos[i] + 1) - origin[i]) / direction[i];
        } else {
            tMax[i] = (static_cast<float>(pos[i]) - origin[i]) / direction[i];
        }
    }
    
    float t = 0.0f;
    int lastAxis = -1;
    
    while (t < localRay.maxDistance) {
        // Check bounds
        if (pos.x >= minBlock.x && pos.x <= maxBlock.x &&
            pos.y >= minBlock.y && pos.y <= maxBlock.y &&
            pos.z >= minBlock.z && pos.z <= maxBlock.z) {
            
            if (isBlockSolid(pos.x, pos.y, pos.z)) {
                hit.hit = true;
                hit.distance = t;
                hit.point = localRay.getPoint(t);
                hit.blockPos = pos;
                
                // Compute normal based on which face we entered
                hit.normal = glm::vec3(0.0f);
                hit.blockNormal = glm::ivec3(0);
                if (lastAxis >= 0) {
                    hit.normal[lastAxis] = -static_cast<float>(step[lastAxis]);
                    hit.blockNormal[lastAxis] = -step[lastAxis];
                }
                
                return true;
            }
        }
        
        // Step to next voxel
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            t = tMax.x;
            pos.x += step.x;
            tMax.x += tDelta.x;
            lastAxis = 0;
        } else if (tMax.y < tMax.z) {
            t = tMax.y;
            pos.y += step.y;
            tMax.y += tDelta.y;
            lastAxis = 1;
        } else {
            t = tMax.z;
            pos.z += step.z;
            tMax.z += tDelta.z;
            lastAxis = 2;
        }
    }
    
    return false;
}

glm::vec3 VoxelCollider::support(const glm::vec3& direction) const {
    // For voxel collider, return corner of bounding box in direction
    return offset + glm::vec3(
        direction.x >= 0.0f ? static_cast<float>(maxBlock.x + 1) : static_cast<float>(minBlock.x),
        direction.y >= 0.0f ? static_cast<float>(maxBlock.y + 1) : static_cast<float>(minBlock.y),
        direction.z >= 0.0f ? static_cast<float>(maxBlock.z + 1) : static_cast<float>(minBlock.z)
    );
}

bool VoxelCollider::isBlockSolid(int x, int y, int z) const {
    if (blockCheck) {
        return blockCheck(x, y, z);
    }
    return false;
}

std::vector<AABB> VoxelCollider::getCollidingBlocks(const AABB& queryAABB) const {
    std::vector<AABB> result;
    
    glm::ivec3 queryMin = glm::ivec3(glm::floor(queryAABB.min - offset));
    glm::ivec3 queryMax = glm::ivec3(glm::floor(queryAABB.max - offset));
    
    // Clamp to voxel bounds
    queryMin = glm::max(queryMin, minBlock);
    queryMax = glm::min(queryMax, maxBlock);
    
    for (int x = queryMin.x; x <= queryMax.x; x++) {
        for (int y = queryMin.y; y <= queryMax.y; y++) {
            for (int z = queryMin.z; z <= queryMax.z; z++) {
                if (isBlockSolid(x, y, z)) {
                    AABB blockAABB(
                        glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) + offset,
                        glm::vec3(static_cast<float>(x + 1), static_cast<float>(y + 1), static_cast<float>(z + 1)) + offset
                    );
                    
                    if (blockAABB.intersects(queryAABB)) {
                        result.push_back(blockAABB);
                    }
                }
            }
        }
    }
    
    return result;
}

} // namespace Physics

