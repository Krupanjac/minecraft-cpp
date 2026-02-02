#pragma once

#include "PhysicsTypes.h"
#include <memory>
#include <functional>

namespace Physics {

// Forward declarations
class Collider;

// ============================================================================
// RigidBody - Dynamic physics object
// ============================================================================
class RigidBody {
public:
    enum class Type {
        Static,     // Immovable, infinite mass (walls, terrain)
        Kinematic,  // Moved by code, affects dynamics but not affected
        Dynamic     // Fully simulated
    };

    RigidBody();
    explicit RigidBody(Type type);
    ~RigidBody() = default;
    
    // Disable copy, allow move
    RigidBody(const RigidBody&) = delete;
    RigidBody& operator=(const RigidBody&) = delete;
    RigidBody(RigidBody&&) = default;
    RigidBody& operator=(RigidBody&&) = default;

    // ========== Transform ==========
    void setPosition(const glm::vec3& pos);
    const glm::vec3& getPosition() const { return position; }
    
    void setRotation(const glm::quat& rot);
    const glm::quat& getRotation() const { return rotation; }
    
    void setTransform(const glm::vec3& pos, const glm::quat& rot);
    glm::mat4 getTransformMatrix() const;
    
    // ========== Velocity ==========
    void setLinearVelocity(const glm::vec3& vel);
    const glm::vec3& getLinearVelocity() const { return linearVelocity; }
    
    void setAngularVelocity(const glm::vec3& vel);
    const glm::vec3& getAngularVelocity() const { return angularVelocity; }
    
    glm::vec3 getVelocityAtPoint(const glm::vec3& worldPoint) const;
    
    // ========== Forces ==========
    void applyForce(const glm::vec3& force);
    void applyForceAtPoint(const glm::vec3& force, const glm::vec3& worldPoint);
    void applyTorque(const glm::vec3& torque);
    void applyImpulse(const glm::vec3& impulse);
    void applyImpulseAtPoint(const glm::vec3& impulse, const glm::vec3& worldPoint);
    void clearForces();
    
    const glm::vec3& getAccumulatedForce() const { return accumulatedForce; }
    const glm::vec3& getAccumulatedTorque() const { return accumulatedTorque; }
    
    // ========== Mass Properties ==========
    void setMass(float mass);
    float getMass() const { return mass; }
    float getInverseMass() const { return inverseMass; }
    
    void setInertia(const glm::mat3& inertia);
    const glm::mat3& getInertia() const { return inertia; }
    const glm::mat3& getInverseInertia() const { return inverseInertia; }
    glm::mat3 getWorldInverseInertia() const;
    
    // ========== Material ==========
    void setMaterial(const PhysicsMaterial& mat) { material = mat; setMass(mat.mass); }
    const PhysicsMaterial& getMaterial() const { return material; }
    PhysicsMaterial& getMaterial() { return material; }
    
    // ========== Type ==========
    void setType(Type t);
    Type getType() const { return type; }
    bool isStatic() const { return type == Type::Static; }
    bool isKinematic() const { return type == Type::Kinematic; }
    bool isDynamic() const { return type == Type::Dynamic; }
    
    // ========== Collision ==========
    void setCollider(std::shared_ptr<Collider> col) { collider = col; }
    std::shared_ptr<Collider> getCollider() const { return collider; }
    
    void setCollisionLayer(CollisionLayer layer) { collisionLayer = layer; }
    CollisionLayer getCollisionLayer() const { return collisionLayer; }
    
    void setCollisionMask(CollisionLayer mask) { collisionMask = mask; }
    CollisionLayer getCollisionMask() const { return collisionMask; }
    
    bool canCollideWith(const RigidBody& other) const;
    
    // ========== Sleeping ==========
    void setAwake(bool awake);
    bool isAwake() const { return awake; }
    bool canSleep() const { return canSleepFlag; }
    void setCanSleep(bool can) { canSleepFlag = can; }
    
    // ========== Gravity ==========
    void setGravityScale(float scale) { gravityScale = scale; }
    float getGravityScale() const { return gravityScale; }
    void setUseGravity(bool use) { useGravity = use; }
    bool getUseGravity() const { return useGravity; }
    
    // ========== Callbacks ==========
    void setOnCollision(CollisionCallback cb) { onCollision = cb; }
    void setOnTriggerEnter(TriggerCallback cb) { onTriggerEnter = cb; }
    void setOnTriggerExit(TriggerCallback cb) { onTriggerExit = cb; }
    
    // ========== Integration (called by PhysicsWorld) ==========
    void integrateForces(float dt, const glm::vec3& gravity);
    void integrateVelocity(float dt);
    void updateSleepState(float dt, float linearThreshold, float angularThreshold, float timeThreshold);
    
    // ========== AABB ==========
    AABB getWorldAABB() const;
    
    // ========== User Data ==========
    void setUserData(void* data) { userData = data; }
    void* getUserData() const { return userData; }
    
    template<typename T>
    T* getUserDataAs() const { return static_cast<T*>(userData); }

    // ========== ID ==========
    uint32_t getId() const { return id; }
    void setId(uint32_t newId) { id = newId; }
    
    // ========== State for solver ==========
    glm::vec3 deltaLinearVelocity;  // Accumulated velocity change from solver
    glm::vec3 deltaAngularVelocity;
    
private:
    // Transform
    glm::vec3 position;
    glm::quat rotation;
    
    // Velocity
    glm::vec3 linearVelocity;
    glm::vec3 angularVelocity;
    
    // Force accumulators (cleared each frame)
    glm::vec3 accumulatedForce;
    glm::vec3 accumulatedTorque;
    
    // Mass properties
    float mass;
    float inverseMass;
    glm::mat3 inertia;
    glm::mat3 inverseInertia;
    
    // Material
    PhysicsMaterial material;
    
    // Type
    Type type;
    
    // Collision
    std::shared_ptr<Collider> collider;
    CollisionLayer collisionLayer;
    CollisionLayer collisionMask;
    
    // Sleeping
    bool awake;
    bool canSleepFlag;
    float sleepTimer;
    
    // Gravity
    float gravityScale;
    bool useGravity;
    
    // Callbacks
    CollisionCallback onCollision;
    TriggerCallback onTriggerEnter;
    TriggerCallback onTriggerExit;
    
    // User data
    void* userData;
    
    // ID
    uint32_t id;
    static uint32_t nextId;
};

} // namespace Physics
