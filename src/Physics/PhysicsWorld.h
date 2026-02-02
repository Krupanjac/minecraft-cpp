#pragma once

#include "PhysicsTypes.h"
#include "RigidBody.h"
#include "Collider.h"
#include "CollisionDetection.h"
#include "Broadphase.h"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Physics {

// ============================================================================
// Contact Constraint - For iterative solver
// ============================================================================
struct ContactConstraint {
    RigidBody* bodyA;
    RigidBody* bodyB;
    glm::vec3 point;
    glm::vec3 normal;
    float penetration;
    float friction;
    float restitution;
    
    // Solver data
    float normalMass;
    float tangentMass1;
    float tangentMass2;
    glm::vec3 tangent1;
    glm::vec3 tangent2;
    
    // Accumulated impulses (warm starting)
    float normalImpulse = 0.0f;
    float tangentImpulse1 = 0.0f;
    float tangentImpulse2 = 0.0f;
    
    // For position correction
    float bias = 0.0f;
};

// ============================================================================
// Contact Manifold - Groups contacts between a pair of bodies
// ============================================================================
struct ContactManifold {
    RigidBody* bodyA = nullptr;
    RigidBody* bodyB = nullptr;
    std::vector<ContactConstraint> contacts;
    
    // For persistent contact tracking
    uint64_t pairId = 0;
    int framesSinceUpdate = 0;
};

// ============================================================================
// Physics World - Main simulation manager
// ============================================================================
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld() = default;
    
    // ========== Configuration ==========
    void setConfig(const PhysicsConfig& config) { this->config = config; }
    const PhysicsConfig& getConfig() const { return config; }
    PhysicsConfig& getConfig() { return config; }
    
    void setGravity(const glm::vec3& g) { config.gravity = g; }
    glm::vec3 getGravity() const { return config.gravity; }
    
    // ========== Body Management ==========
    RigidBody* createBody(RigidBody::Type type = RigidBody::Type::Dynamic);
    void destroyBody(RigidBody* body);
    
    const std::vector<std::unique_ptr<RigidBody>>& getBodies() const { return bodies; }
    size_t getBodyCount() const { return bodies.size(); }
    
    // ========== Simulation ==========
    void step(float deltaTime);
    void stepFixed(float fixedDt);
    
    // ========== Queries ==========
    
    // Raycast - returns first hit
    bool raycast(const Ray& ray, RaycastHit& hit, 
                 CollisionLayer mask = CollisionLayer::All) const;
    
    // Raycast all - returns all hits sorted by distance
    bool raycastAll(const Ray& ray, std::vector<RaycastHit>& hits,
                    CollisionLayer mask = CollisionLayer::All) const;
    
    // AABB query - get all bodies intersecting an AABB
    void queryAABB(const AABB& aabb, std::vector<RigidBody*>& results,
                   CollisionLayer mask = CollisionLayer::All) const;
    
    // Sphere query
    void querySphere(const glm::vec3& center, float radius, std::vector<RigidBody*>& results,
                     CollisionLayer mask = CollisionLayer::All) const;
    
    // ========== Callbacks ==========
    using CollisionBeginCallback = std::function<void(RigidBody*, RigidBody*, const std::vector<Contact>&)>;
    using CollisionEndCallback = std::function<void(RigidBody*, RigidBody*)>;
    using CollisionStayCallback = std::function<void(RigidBody*, RigidBody*, const std::vector<Contact>&)>;
    
    void setCollisionBeginCallback(CollisionBeginCallback cb) { onCollisionBegin = cb; }
    void setCollisionEndCallback(CollisionEndCallback cb) { onCollisionEnd = cb; }
    void setCollisionStayCallback(CollisionStayCallback cb) { onCollisionStay = cb; }
    
    // ========== Debug ==========
    void setDebugDraw(bool enable) { debugDraw = enable; }
    bool getDebugDraw() const { return debugDraw; }
    
    // Get all contact points for debug rendering
    const std::vector<ContactManifold>& getContactManifolds() const { return manifolds; }
    
    // Statistics
    struct Stats {
        int bodyCount = 0;
        int activeBodyCount = 0;
        int contactCount = 0;
        int broadphasePairs = 0;
        float lastStepTime = 0.0f;
    };
    const Stats& getStats() const { return stats; }
    
    // ========== Voxel World Integration ==========
    using BlockQueryFunc = std::function<bool(int x, int y, int z)>;
    void setBlockQueryFunction(BlockQueryFunc func) { blockQuery = func; }
    
    // Check collision between a body and the voxel world
    bool collideWithWorld(RigidBody* body, std::vector<Contact>& contacts) const;
    
private:
    PhysicsConfig config;
    std::vector<std::unique_ptr<RigidBody>> bodies;
    mutable Broadphase broadphase;
    
    // Contact manifolds (persistent across frames)
    std::vector<ContactManifold> manifolds;
    std::unordered_map<uint64_t, size_t> manifoldMap; // pairId -> index
    
    // Callbacks
    CollisionBeginCallback onCollisionBegin;
    CollisionEndCallback onCollisionEnd;
    CollisionStayCallback onCollisionStay;
    
    // Voxel world query
    BlockQueryFunc blockQuery;
    
    // Debug
    bool debugDraw = false;
    Stats stats;
    
    // Accumulator for fixed timestep
    float timeAccumulator = 0.0f;
    
    // ========== Internal Methods ==========
    
    // Broad phase: find potential collision pairs
    void broadphaseCollision(std::vector<CollisionPair>& pairs);
    
    // Narrow phase: generate contacts
    void narrowphaseCollision(const std::vector<CollisionPair>& pairs);
    
    // Solve velocity constraints
    void solveVelocityConstraints();
    
    // Solve position constraints (position correction)
    void solvePositionConstraints();
    
    // Apply forces (gravity, etc.)
    void integrateForces(float dt);
    
    // Integrate velocities to update positions
    void integrateVelocities(float dt);
    
    // Update sleeping state
    void updateSleeping(float dt);
    
    // Generate a unique ID for a body pair
    static uint64_t makePairId(RigidBody* a, RigidBody* b);
    
    // Setup constraint data
    void warmStart();
    void setupConstraint(ContactConstraint& c);
    
    // Solve a single contact constraint
    void solveContactVelocity(ContactConstraint& c);
    bool solveContactPosition(ContactConstraint& c);
    
    // Clean up old manifolds
    void pruneManifolds();
};

} // namespace Physics
