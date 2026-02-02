#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <vector>
#include <functional>

namespace Physics {

// Forward declarations
class RigidBody;
class Collider;

// ============================================================================
// Physics Material - Defines physical properties of objects
// ============================================================================
struct PhysicsMaterial {
    float mass = 1.0f;              // Mass in kg (0 = static/infinite mass)
    float friction = 0.5f;          // Coefficient of friction [0, 1]
    float restitution = 0.3f;       // Bounciness [0, 1] (0 = no bounce, 1 = perfect bounce)
    float linearDamping = 0.01f;    // Air resistance for linear motion
    float angularDamping = 0.05f;   // Air resistance for rotation
    
    // Destruction properties
    float hardness = 1.0f;          // Resistance to breaking [0, inf)
    float blastResistance = 1.0f;   // Resistance to explosions [0, inf)
    
    static PhysicsMaterial Stone() {
        return { 2.5f, 0.7f, 0.2f, 0.01f, 0.05f, 5.0f, 6.0f };
    }
    static PhysicsMaterial Wood() {
        return { 0.8f, 0.6f, 0.3f, 0.01f, 0.05f, 2.0f, 2.0f };
    }
    static PhysicsMaterial Dirt() {
        return { 1.5f, 0.8f, 0.1f, 0.01f, 0.05f, 0.5f, 0.5f };
    }
    static PhysicsMaterial Metal() {
        return { 7.8f, 0.4f, 0.4f, 0.01f, 0.05f, 10.0f, 12.0f };
    }
    static PhysicsMaterial Glass() {
        return { 2.5f, 0.5f, 0.2f, 0.01f, 0.05f, 0.3f, 0.3f };
    }
    static PhysicsMaterial Rubber() {
        return { 1.1f, 0.9f, 0.8f, 0.02f, 0.08f, 1.0f, 1.0f };
    }
};

// ============================================================================
// Collision Layer - For filtering which objects can collide
// ============================================================================
enum class CollisionLayer : uint32_t {
    None        = 0,
    Default     = 1 << 0,
    Player      = 1 << 1,
    Entity      = 1 << 2,
    Block       = 1 << 3,
    Debris      = 1 << 4,
    Projectile  = 1 << 5,
    Trigger     = 1 << 6,
    Water       = 1 << 7,
    All         = 0xFFFFFFFF
};

inline CollisionLayer operator|(CollisionLayer a, CollisionLayer b) {
    return static_cast<CollisionLayer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline CollisionLayer operator&(CollisionLayer a, CollisionLayer b) {
    return static_cast<CollisionLayer>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasLayer(CollisionLayer mask, CollisionLayer layer) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(layer)) != 0;
}

// ============================================================================
// AABB - Axis-Aligned Bounding Box
// ============================================================================
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    
    AABB() : min(0.0f), max(0.0f) {}
    AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}
    
    // Create from center and half-extents
    static AABB fromCenterExtents(const glm::vec3& center, const glm::vec3& halfExtents) {
        return AABB(center - halfExtents, center + halfExtents);
    }
    
    glm::vec3 getCenter() const { return (min + max) * 0.5f; }
    glm::vec3 getExtents() const { return (max - min) * 0.5f; }
    glm::vec3 getSize() const { return max - min; }
    float getVolume() const { 
        glm::vec3 s = getSize();
        return s.x * s.y * s.z;
    }
    
    bool contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
    
    bool intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
    
    AABB expanded(float amount) const {
        return AABB(min - glm::vec3(amount), max + glm::vec3(amount));
    }
    
    AABB merged(const AABB& other) const {
        return AABB(glm::min(min, other.min), glm::max(max, other.max));
    }
    
    // Transform AABB (conservative, may be larger than necessary)
    AABB transformed(const glm::mat4& transform) const;
};

// ============================================================================
// Ray - For raycasting
// ============================================================================
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    float maxDistance = 1000.0f;
    
    Ray() : origin(0.0f), direction(0.0f, 0.0f, -1.0f) {}
    Ray(const glm::vec3& origin, const glm::vec3& direction, float maxDist = 1000.0f)
        : origin(origin), direction(glm::normalize(direction)), maxDistance(maxDist) {}
    
    glm::vec3 getPoint(float t) const { return origin + direction * t; }
};

// ============================================================================
// Collision Contact - Information about a collision point
// ============================================================================
struct Contact {
    glm::vec3 point;           // World-space contact point
    glm::vec3 normal;          // Contact normal (from A to B)
    float penetration = 0.0f;  // Penetration depth (positive = overlapping)
    
    // Bodies involved (may be null for static geometry)
    RigidBody* bodyA = nullptr;
    RigidBody* bodyB = nullptr;
    
    // Friction and restitution (combined from materials)
    float friction = 0.5f;
    float restitution = 0.3f;
};

// ============================================================================
// Collision Result - Full collision information
// ============================================================================
struct CollisionResult {
    bool hit = false;
    std::vector<Contact> contacts;
    
    // For raycasts
    float distance = 0.0f;
    glm::vec3 hitPoint;
    glm::vec3 hitNormal;
    RigidBody* hitBody = nullptr;
    
    operator bool() const { return hit; }
};

// ============================================================================
// Raycast Hit - Result of a raycast
// ============================================================================
struct RaycastHit {
    bool hit = false;
    float distance = 0.0f;
    glm::vec3 point;
    glm::vec3 normal;
    RigidBody* body = nullptr;
    Collider* collider = nullptr;
    
    // For voxel raycasts
    glm::ivec3 blockPos;
    glm::ivec3 blockNormal;
    
    operator bool() const { return hit; }
};

// ============================================================================
// Sweep Result - For continuous collision detection
// ============================================================================
struct SweepResult {
    bool hit = false;
    float time = 1.0f;         // Time of impact [0, 1]
    glm::vec3 point;           // Point of first contact
    glm::vec3 normal;          // Surface normal at contact
    RigidBody* body = nullptr;
    
    operator bool() const { return hit; }
};

// ============================================================================
// Collision Callback Types
// ============================================================================
using CollisionCallback = std::function<void(const Contact&)>;
using TriggerCallback = std::function<void(RigidBody*, RigidBody*)>;

// ============================================================================
// Physics Configuration
// ============================================================================
struct PhysicsConfig {
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);  // Gravity acceleration
    float fixedTimeStep = 1.0f / 60.0f;                   // Physics update rate
    int maxSubSteps = 4;                                  // Max physics steps per frame
    int velocityIterations = 8;                           // Constraint solver iterations
    int positionIterations = 3;                           // Position correction iterations
    
    // Sleeping thresholds
    float sleepLinearThreshold = 0.1f;
    float sleepAngularThreshold = 0.1f;
    float sleepTimeThreshold = 0.5f;
    
    // Collision settings
    float contactBreakingThreshold = 0.02f;
    float allowedPenetration = 0.01f;
    float baumgarteScale = 0.2f;  // Position correction strength
    
    // Continuous collision detection
    bool enableCCD = true;
    float ccdThreshold = 0.5f;  // Min velocity for CCD
};

// ============================================================================
// Inertia Tensor Utilities
// ============================================================================
namespace Inertia {
    // Solid box inertia tensor
    inline glm::mat3 solidBox(float mass, const glm::vec3& halfExtents) {
        float x2 = halfExtents.x * halfExtents.x;
        float y2 = halfExtents.y * halfExtents.y;
        float z2 = halfExtents.z * halfExtents.z;
        float factor = mass / 3.0f;
        return glm::mat3(
            factor * (y2 + z2), 0.0f, 0.0f,
            0.0f, factor * (x2 + z2), 0.0f,
            0.0f, 0.0f, factor * (x2 + y2)
        );
    }
    
    // Solid sphere inertia tensor
    inline glm::mat3 solidSphere(float mass, float radius) {
        float i = (2.0f / 5.0f) * mass * radius * radius;
        return glm::mat3(i);
    }
    
    // Hollow sphere inertia tensor
    inline glm::mat3 hollowSphere(float mass, float radius) {
        float i = (2.0f / 3.0f) * mass * radius * radius;
        return glm::mat3(i);
    }
    
    // Solid cylinder (along Y axis) inertia tensor
    inline glm::mat3 solidCylinder(float mass, float radius, float height) {
        float r2 = radius * radius;
        float h2 = height * height;
        float iX = mass * (3.0f * r2 + h2) / 12.0f;
        float iY = mass * r2 / 2.0f;
        return glm::mat3(
            iX, 0.0f, 0.0f,
            0.0f, iY, 0.0f,
            0.0f, 0.0f, iX
        );
    }
}

} // namespace Physics
