#pragma once

#include "PhysicsTypes.h"
#include <memory>
#include <vector>

namespace Physics {

// ============================================================================
// Collider Base Class
// ============================================================================
class Collider {
public:
    enum class Type {
        Sphere,
        Box,
        Capsule,
        Mesh,      // For complex shapes
        Voxel      // Special case for voxel terrain
    };
    
    Collider(Type type) : colliderType(type), isTrigger(false), offset(0.0f) {}
    virtual ~Collider() = default;
    
    Type getType() const { return colliderType; }
    
    // Get local-space AABB (without transform)
    virtual AABB getLocalAABB() const = 0;
    
    // Get world-space AABB (with transform applied)
    virtual AABB getWorldAABB(const glm::mat4& transform) const;
    
    // Check if point is inside collider (in local space)
    virtual bool containsPoint(const glm::vec3& localPoint) const = 0;
    
    // Get closest point on surface (in local space)
    virtual glm::vec3 getClosestPoint(const glm::vec3& localPoint) const = 0;
    
    // Raycast in local space
    virtual bool raycast(const Ray& localRay, RaycastHit& hit) const = 0;
    
    // Support function for GJK/EPA algorithms
    virtual glm::vec3 support(const glm::vec3& direction) const = 0;
    
    // Triggers don't generate collision response, only events
    void setTrigger(bool trigger) { isTrigger = trigger; }
    bool getTrigger() const { return isTrigger; }
    
    // Local offset from rigid body center
    void setOffset(const glm::vec3& off) { offset = off; }
    const glm::vec3& getOffset() const { return offset; }
    
protected:
    Type colliderType;
    bool isTrigger;
    glm::vec3 offset;
};

// ============================================================================
// Sphere Collider
// ============================================================================
class SphereCollider : public Collider {
public:
    explicit SphereCollider(float radius = 0.5f);
    
    float getRadius() const { return radius; }
    void setRadius(float r) { radius = r; }
    
    AABB getLocalAABB() const override;
    bool containsPoint(const glm::vec3& localPoint) const override;
    glm::vec3 getClosestPoint(const glm::vec3& localPoint) const override;
    bool raycast(const Ray& localRay, RaycastHit& hit) const override;
    glm::vec3 support(const glm::vec3& direction) const override;
    
private:
    float radius;
};

// ============================================================================
// Box Collider
// ============================================================================
class BoxCollider : public Collider {
public:
    explicit BoxCollider(const glm::vec3& halfExtents = glm::vec3(0.5f));
    
    const glm::vec3& getHalfExtents() const { return halfExtents; }
    void setHalfExtents(const glm::vec3& extents) { halfExtents = extents; }
    
    AABB getLocalAABB() const override;
    bool containsPoint(const glm::vec3& localPoint) const override;
    glm::vec3 getClosestPoint(const glm::vec3& localPoint) const override;
    bool raycast(const Ray& localRay, RaycastHit& hit) const override;
    glm::vec3 support(const glm::vec3& direction) const override;
    
private:
    glm::vec3 halfExtents;
};

// ============================================================================
// Capsule Collider (useful for characters)
// ============================================================================
class CapsuleCollider : public Collider {
public:
    CapsuleCollider(float radius = 0.5f, float height = 2.0f);
    
    float getRadius() const { return radius; }
    void setRadius(float r) { radius = r; }
    
    float getHeight() const { return height; }
    void setHeight(float h) { height = h; }
    
    AABB getLocalAABB() const override;
    bool containsPoint(const glm::vec3& localPoint) const override;
    glm::vec3 getClosestPoint(const glm::vec3& localPoint) const override;
    bool raycast(const Ray& localRay, RaycastHit& hit) const override;
    glm::vec3 support(const glm::vec3& direction) const override;
    
private:
    float radius;
    float height; // Total height including caps
    
    // Get the two endpoints of the capsule's line segment
    void getSegmentEndpoints(glm::vec3& p0, glm::vec3& p1) const;
};

// ============================================================================
// Compound Collider (multiple shapes combined)
// ============================================================================
class CompoundCollider : public Collider {
public:
    struct ChildCollider {
        std::shared_ptr<Collider> collider;
        glm::vec3 position;
        glm::quat rotation;
    };
    
    CompoundCollider();
    
    void addCollider(std::shared_ptr<Collider> collider, 
                     const glm::vec3& position = glm::vec3(0.0f),
                     const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    
    void removeCollider(size_t index);
    void clearColliders();
    
    const std::vector<ChildCollider>& getChildren() const { return children; }
    
    AABB getLocalAABB() const override;
    bool containsPoint(const glm::vec3& localPoint) const override;
    glm::vec3 getClosestPoint(const glm::vec3& localPoint) const override;
    bool raycast(const Ray& localRay, RaycastHit& hit) const override;
    glm::vec3 support(const glm::vec3& direction) const override;
    
private:
    std::vector<ChildCollider> children;
    mutable AABB cachedAABB;
    mutable bool aabbDirty = true;
};

// ============================================================================
// Voxel Collider - Special collider for chunk terrain
// ============================================================================
class VoxelCollider : public Collider {
public:
    // Callback to check if a block position is solid
    using BlockCheckFunc = std::function<bool(int x, int y, int z)>;
    
    VoxelCollider(const glm::ivec3& minBlock, const glm::ivec3& maxBlock, BlockCheckFunc checkFunc);
    
    AABB getLocalAABB() const override;
    bool containsPoint(const glm::vec3& localPoint) const override;
    glm::vec3 getClosestPoint(const glm::vec3& localPoint) const override;
    bool raycast(const Ray& localRay, RaycastHit& hit) const override;
    glm::vec3 support(const glm::vec3& direction) const override;
    
    // Voxel-specific methods
    bool isBlockSolid(int x, int y, int z) const;
    
    // Get all solid blocks in a region that intersect with an AABB
    std::vector<AABB> getCollidingBlocks(const AABB& queryAABB) const;
    
private:
    glm::ivec3 minBlock;
    glm::ivec3 maxBlock;
    BlockCheckFunc blockCheck;
};

} // namespace Physics
