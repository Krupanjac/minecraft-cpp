#include "PhysicsWorld.h"
#include <algorithm>
#include <chrono>

namespace Physics {

PhysicsWorld::PhysicsWorld()
    : broadphase(Broadphase::Algorithm::Hybrid)
{
}

RigidBody* PhysicsWorld::createBody(RigidBody::Type type) {
    auto body = std::make_unique<RigidBody>(type);
    RigidBody* ptr = body.get();
    bodies.push_back(std::move(body));
    broadphase.add(ptr);
    return ptr;
}

void PhysicsWorld::destroyBody(RigidBody* body) {
    broadphase.remove(body);
    
    // Remove from manifolds
    for (auto it = manifolds.begin(); it != manifolds.end(); ) {
        if (it->bodyA == body || it->bodyB == body) {
            manifoldMap.erase(it->pairId);
            it = manifolds.erase(it);
        } else {
            ++it;
        }
    }
    
    // Remove from bodies list
    auto it = std::find_if(bodies.begin(), bodies.end(),
                           [body](const auto& b) { return b.get() == body; });
    if (it != bodies.end()) {
        bodies.erase(it);
    }
}

void PhysicsWorld::step(float deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    timeAccumulator += deltaTime;
    
    int steps = 0;
    while (timeAccumulator >= config.fixedTimeStep && steps < config.maxSubSteps) {
        stepFixed(config.fixedTimeStep);
        timeAccumulator -= config.fixedTimeStep;
        steps++;
    }
    
    // Interpolate remaining time if needed (for smooth rendering)
    
    auto endTime = std::chrono::high_resolution_clock::now();
    stats.lastStepTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void PhysicsWorld::stepFixed(float dt) {
    // 1. Apply external forces (gravity)
    integrateForces(dt);
    
    // 2. Broad phase collision detection
    std::vector<CollisionPair> pairs;
    broadphaseCollision(pairs);
    stats.broadphasePairs = static_cast<int>(pairs.size());
    
    // 3. Narrow phase collision detection
    narrowphaseCollision(pairs);
    
    // 4. Warm start (apply previous frame's impulses)
    warmStart();
    
    // 5. Solve velocity constraints (iteratively)
    for (int i = 0; i < config.velocityIterations; i++) {
        solveVelocityConstraints();
    }
    
    // 6. Integrate velocities to update positions
    integrateVelocities(dt);
    
    // 7. Solve position constraints (position correction)
    for (int i = 0; i < config.positionIterations; i++) {
        solvePositionConstraints();
    }
    
    // 8. Update broadphase
    for (auto& body : bodies) {
        if (body->isAwake()) {
            broadphase.update(body.get());
        }
    }
    
    // 9. Update sleeping state
    updateSleeping(dt);
    
    // 10. Cleanup old manifolds
    pruneManifolds();
    
    // Update stats
    stats.bodyCount = static_cast<int>(bodies.size());
    stats.activeBodyCount = 0;
    stats.contactCount = 0;
    
    for (const auto& body : bodies) {
        if (body->isAwake()) stats.activeBodyCount++;
    }
    for (const auto& manifold : manifolds) {
        stats.contactCount += static_cast<int>(manifold.contacts.size());
    }
}

void PhysicsWorld::integrateForces(float dt) {
    for (auto& body : bodies) {
        body->integrateForces(dt, config.gravity);
    }
}

void PhysicsWorld::integrateVelocities(float dt) {
    for (auto& body : bodies) {
        body->integrateVelocity(dt);
    }
}

void PhysicsWorld::updateSleeping(float dt) {
    for (auto& body : bodies) {
        body->updateSleepState(dt, config.sleepLinearThreshold, 
                               config.sleepAngularThreshold, config.sleepTimeThreshold);
    }
}

void PhysicsWorld::broadphaseCollision(std::vector<CollisionPair>& pairs) {
    broadphase.getPotentialPairs(pairs);
}

void PhysicsWorld::narrowphaseCollision(const std::vector<CollisionPair>& pairs) {
    // Mark all manifolds as stale
    for (auto& manifold : manifolds) {
        manifold.framesSinceUpdate++;
    }
    
    for (const auto& pair : pairs) {
        if (!pair.bodyA->getCollider() || !pair.bodyB->getCollider()) {
            continue;
        }
        
        // Check against voxel world too
        std::vector<Contact> contacts;
        
        // Collider vs Collider
        CollisionDetection::collide(
            *pair.bodyA->getCollider(), pair.bodyA->getTransformMatrix(),
            *pair.bodyB->getCollider(), pair.bodyB->getTransformMatrix(),
            contacts
        );
        
        if (contacts.empty()) {
            continue;
        }
        
        // Find or create manifold
        uint64_t pairId = makePairId(pair.bodyA, pair.bodyB);
        
        auto it = manifoldMap.find(pairId);
        ContactManifold* manifold = nullptr;
        bool isNewManifold = false;
        
        if (it != manifoldMap.end()) {
            manifold = &manifolds[it->second];
        } else {
            manifolds.push_back(ContactManifold());
            manifold = &manifolds.back();
            manifold->bodyA = pair.bodyA;
            manifold->bodyB = pair.bodyB;
            manifold->pairId = pairId;
            manifoldMap[pairId] = manifolds.size() - 1;
            isNewManifold = true;
        }
        
        manifold->framesSinceUpdate = 0;
        
        // Build contact constraints
        manifold->contacts.clear();
        
        for (const auto& contact : contacts) {
            ContactConstraint c;
            c.bodyA = pair.bodyA;
            c.bodyB = pair.bodyB;
            c.point = contact.point;
            c.normal = contact.normal;
            c.penetration = contact.penetration;
            
            // Combine materials
            const auto& matA = pair.bodyA->getMaterial();
            const auto& matB = pair.bodyB->getMaterial();
            c.friction = std::sqrt(matA.friction * matB.friction);
            c.restitution = std::max(matA.restitution, matB.restitution);
            
            setupConstraint(c);
            manifold->contacts.push_back(c);
        }
        
        // Fire callbacks
        if (isNewManifold && onCollisionBegin) {
            onCollisionBegin(pair.bodyA, pair.bodyB, contacts);
        } else if (!isNewManifold && onCollisionStay) {
            onCollisionStay(pair.bodyA, pair.bodyB, contacts);
        }
    }
}

void PhysicsWorld::setupConstraint(ContactConstraint& c) {
    // Compute relative velocity at contact point
    glm::vec3 rA = c.point - c.bodyA->getPosition();
    glm::vec3 rB = c.point - c.bodyB->getPosition();
    
    glm::vec3 relVel = c.bodyB->getVelocityAtPoint(c.point) - c.bodyA->getVelocityAtPoint(c.point);
    float velAlongNormal = glm::dot(relVel, c.normal);
    
    // Compute effective mass for normal direction
    float invMassSum = c.bodyA->getInverseMass() + c.bodyB->getInverseMass();
    
    glm::vec3 rnA = glm::cross(rA, c.normal);
    glm::vec3 rnB = glm::cross(rB, c.normal);
    
    float kNormal = invMassSum +
        glm::dot(rnA, c.bodyA->getWorldInverseInertia() * rnA) +
        glm::dot(rnB, c.bodyB->getWorldInverseInertia() * rnB);
    
    c.normalMass = (kNormal > 0.0f) ? 1.0f / kNormal : 0.0f;
    
    // Compute bias for position correction (Baumgarte stabilization)
    c.bias = -config.baumgarteScale / config.fixedTimeStep * 
             std::max(c.penetration - config.allowedPenetration, 0.0f);
    
    // Add restitution bias
    if (velAlongNormal < -1.0f) {
        c.bias += c.restitution * velAlongNormal;
    }
    
    // Compute tangent directions for friction
    glm::vec3 t = relVel - c.normal * velAlongNormal;
    if (glm::dot(t, t) > 1e-8f) {
        c.tangent1 = glm::normalize(t);
    } else {
        // Generate arbitrary tangent
        if (std::abs(c.normal.x) > 0.9f) {
            c.tangent1 = glm::normalize(glm::cross(c.normal, glm::vec3(0.0f, 1.0f, 0.0f)));
        } else {
            c.tangent1 = glm::normalize(glm::cross(c.normal, glm::vec3(1.0f, 0.0f, 0.0f)));
        }
    }
    c.tangent2 = glm::cross(c.normal, c.tangent1);
    
    // Compute effective mass for tangent directions
    glm::vec3 rt1A = glm::cross(rA, c.tangent1);
    glm::vec3 rt1B = glm::cross(rB, c.tangent1);
    float kTangent1 = invMassSum +
        glm::dot(rt1A, c.bodyA->getWorldInverseInertia() * rt1A) +
        glm::dot(rt1B, c.bodyB->getWorldInverseInertia() * rt1B);
    c.tangentMass1 = (kTangent1 > 0.0f) ? 1.0f / kTangent1 : 0.0f;
    
    glm::vec3 rt2A = glm::cross(rA, c.tangent2);
    glm::vec3 rt2B = glm::cross(rB, c.tangent2);
    float kTangent2 = invMassSum +
        glm::dot(rt2A, c.bodyA->getWorldInverseInertia() * rt2A) +
        glm::dot(rt2B, c.bodyB->getWorldInverseInertia() * rt2B);
    c.tangentMass2 = (kTangent2 > 0.0f) ? 1.0f / kTangent2 : 0.0f;
}

void PhysicsWorld::warmStart() {
    for (auto& manifold : manifolds) {
        for (auto& c : manifold.contacts) {
            // Apply accumulated impulse from previous frame
            glm::vec3 P = c.normal * c.normalImpulse + 
                          c.tangent1 * c.tangentImpulse1 + 
                          c.tangent2 * c.tangentImpulse2;
            
            c.bodyA->deltaLinearVelocity -= P * c.bodyA->getInverseMass();
            c.bodyB->deltaLinearVelocity += P * c.bodyB->getInverseMass();
            
            glm::vec3 rA = c.point - c.bodyA->getPosition();
            glm::vec3 rB = c.point - c.bodyB->getPosition();
            
            c.bodyA->deltaAngularVelocity -= c.bodyA->getWorldInverseInertia() * glm::cross(rA, P);
            c.bodyB->deltaAngularVelocity += c.bodyB->getWorldInverseInertia() * glm::cross(rB, P);
        }
    }
}

void PhysicsWorld::solveVelocityConstraints() {
    for (auto& manifold : manifolds) {
        for (auto& c : manifold.contacts) {
            solveContactVelocity(c);
        }
    }
}

void PhysicsWorld::solveContactVelocity(ContactConstraint& c) {
    glm::vec3 rA = c.point - c.bodyA->getPosition();
    glm::vec3 rB = c.point - c.bodyB->getPosition();
    
    // Compute relative velocity
    glm::vec3 velA = c.bodyA->getLinearVelocity() + c.bodyA->deltaLinearVelocity +
                     glm::cross(c.bodyA->getAngularVelocity() + c.bodyA->deltaAngularVelocity, rA);
    glm::vec3 velB = c.bodyB->getLinearVelocity() + c.bodyB->deltaLinearVelocity +
                     glm::cross(c.bodyB->getAngularVelocity() + c.bodyB->deltaAngularVelocity, rB);
    glm::vec3 relVel = velB - velA;
    
    // ========== Solve friction ==========
    
    // Tangent 1
    float vt1 = glm::dot(relVel, c.tangent1);
    float dPt1 = -vt1 * c.tangentMass1;
    
    // Clamp friction
    float maxFriction = c.friction * c.normalImpulse;
    float newPt1 = std::clamp(c.tangentImpulse1 + dPt1, -maxFriction, maxFriction);
    dPt1 = newPt1 - c.tangentImpulse1;
    c.tangentImpulse1 = newPt1;
    
    // Tangent 2
    float vt2 = glm::dot(relVel, c.tangent2);
    float dPt2 = -vt2 * c.tangentMass2;
    
    float newPt2 = std::clamp(c.tangentImpulse2 + dPt2, -maxFriction, maxFriction);
    dPt2 = newPt2 - c.tangentImpulse2;
    c.tangentImpulse2 = newPt2;
    
    // Apply friction impulse
    glm::vec3 Pf = c.tangent1 * dPt1 + c.tangent2 * dPt2;
    
    c.bodyA->deltaLinearVelocity -= Pf * c.bodyA->getInverseMass();
    c.bodyB->deltaLinearVelocity += Pf * c.bodyB->getInverseMass();
    c.bodyA->deltaAngularVelocity -= c.bodyA->getWorldInverseInertia() * glm::cross(rA, Pf);
    c.bodyB->deltaAngularVelocity += c.bodyB->getWorldInverseInertia() * glm::cross(rB, Pf);
    
    // ========== Solve normal constraint ==========
    
    // Recompute relative velocity after friction
    velA = c.bodyA->getLinearVelocity() + c.bodyA->deltaLinearVelocity +
           glm::cross(c.bodyA->getAngularVelocity() + c.bodyA->deltaAngularVelocity, rA);
    velB = c.bodyB->getLinearVelocity() + c.bodyB->deltaLinearVelocity +
           glm::cross(c.bodyB->getAngularVelocity() + c.bodyB->deltaAngularVelocity, rB);
    relVel = velB - velA;
    
    float vn = glm::dot(relVel, c.normal);
    float dPn = (-vn + c.bias) * c.normalMass;
    
    // Clamp normal impulse (can only push, not pull)
    float newPn = std::max(c.normalImpulse + dPn, 0.0f);
    dPn = newPn - c.normalImpulse;
    c.normalImpulse = newPn;
    
    // Apply normal impulse
    glm::vec3 Pn = c.normal * dPn;
    
    c.bodyA->deltaLinearVelocity -= Pn * c.bodyA->getInverseMass();
    c.bodyB->deltaLinearVelocity += Pn * c.bodyB->getInverseMass();
    c.bodyA->deltaAngularVelocity -= c.bodyA->getWorldInverseInertia() * glm::cross(rA, Pn);
    c.bodyB->deltaAngularVelocity += c.bodyB->getWorldInverseInertia() * glm::cross(rB, Pn);
}

void PhysicsWorld::solvePositionConstraints() {
    for (auto& manifold : manifolds) {
        for (auto& c : manifold.contacts) {
            solveContactPosition(c);
        }
    }
}

bool PhysicsWorld::solveContactPosition(ContactConstraint& c) {
    // Recompute penetration (bodies may have moved)
    glm::vec3 rA = c.point - c.bodyA->getPosition();
    glm::vec3 rB = c.point - c.bodyB->getPosition();
    
    float separation = c.penetration - config.allowedPenetration;
    if (separation <= 0.0f) {
        return true; // No correction needed
    }
    
    // Compute correction
    float invMassSum = c.bodyA->getInverseMass() + c.bodyB->getInverseMass();
    if (invMassSum <= 0.0f) return true;
    
    float correction = std::min(config.baumgarteScale * separation, 0.2f);
    glm::vec3 P = c.normal * correction / invMassSum;
    
    // Apply position correction
    if (c.bodyA->isDynamic()) {
        glm::vec3 newPos = c.bodyA->getPosition() - P * c.bodyA->getInverseMass();
        c.bodyA->setPosition(newPos);
    }
    if (c.bodyB->isDynamic()) {
        glm::vec3 newPos = c.bodyB->getPosition() + P * c.bodyB->getInverseMass();
        c.bodyB->setPosition(newPos);
    }
    
    return separation < 3.0f * config.allowedPenetration;
}

void PhysicsWorld::pruneManifolds() {
    // Remove manifolds that haven't been updated recently
    for (auto it = manifolds.begin(); it != manifolds.end(); ) {
        if (it->framesSinceUpdate > 2) {
            // Fire collision end callback
            if (onCollisionEnd) {
                onCollisionEnd(it->bodyA, it->bodyB);
            }
            
            manifoldMap.erase(it->pairId);
            it = manifolds.erase(it);
        } else {
            ++it;
        }
    }
    
    // Rebuild manifold map indices
    manifoldMap.clear();
    for (size_t i = 0; i < manifolds.size(); i++) {
        manifoldMap[manifolds[i].pairId] = i;
    }
}

uint64_t PhysicsWorld::makePairId(RigidBody* a, RigidBody* b) {
    // Ensure consistent ordering
    if (a > b) std::swap(a, b);
    
    uint64_t idA = static_cast<uint64_t>(a->getId());
    uint64_t idB = static_cast<uint64_t>(b->getId());
    return (idA << 32) | idB;
}

bool PhysicsWorld::raycast(const Ray& ray, RaycastHit& hit, CollisionLayer mask) const {
    hit.hit = false;
    hit.distance = ray.maxDistance;
    
    auto callback = [&](RigidBody* body, RaycastHit& bodyHit) -> bool {
        if (!hasLayer(mask, body->getCollisionLayer())) {
            return false;
        }
        
        auto collider = body->getCollider();
        if (!collider) return false;
        
        // Transform ray to local space
        glm::mat4 invTransform = glm::inverse(body->getTransformMatrix());
        glm::vec3 localOrigin = glm::vec3(invTransform * glm::vec4(ray.origin, 1.0f));
        glm::vec3 localDir = glm::normalize(glm::vec3(invTransform * glm::vec4(ray.direction, 0.0f)));
        
        Ray localRay(localOrigin, localDir, ray.maxDistance);
        
        if (collider->raycast(localRay, bodyHit)) {
            // Transform hit back to world space
            bodyHit.point = glm::vec3(body->getTransformMatrix() * glm::vec4(bodyHit.point, 1.0f));
            bodyHit.normal = glm::normalize(glm::vec3(body->getTransformMatrix() * glm::vec4(bodyHit.normal, 0.0f)));
            bodyHit.body = body;
            bodyHit.collider = collider.get();
            return true;
        }
        
        return false;
    };
    
    return broadphase.raycast(ray, hit, callback);
}

bool PhysicsWorld::raycastAll(const Ray& ray, std::vector<RaycastHit>& hits, CollisionLayer mask) const {
    hits.clear();
    
    std::vector<RigidBody*> candidates;
    AABB rayAABB(
        glm::min(ray.origin, ray.getPoint(ray.maxDistance)),
        glm::max(ray.origin, ray.getPoint(ray.maxDistance))
    );
    broadphase.query(rayAABB, candidates);
    
    for (RigidBody* body : candidates) {
        if (!hasLayer(mask, body->getCollisionLayer())) {
            continue;
        }
        
        auto collider = body->getCollider();
        if (!collider) continue;
        
        glm::mat4 invTransform = glm::inverse(body->getTransformMatrix());
        glm::vec3 localOrigin = glm::vec3(invTransform * glm::vec4(ray.origin, 1.0f));
        glm::vec3 localDir = glm::normalize(glm::vec3(invTransform * glm::vec4(ray.direction, 0.0f)));
        
        Ray localRay(localOrigin, localDir, ray.maxDistance);
        RaycastHit bodyHit;
        
        if (collider->raycast(localRay, bodyHit)) {
            bodyHit.point = glm::vec3(body->getTransformMatrix() * glm::vec4(bodyHit.point, 1.0f));
            bodyHit.normal = glm::normalize(glm::vec3(body->getTransformMatrix() * glm::vec4(bodyHit.normal, 0.0f)));
            bodyHit.body = body;
            bodyHit.collider = collider.get();
            hits.push_back(bodyHit);
        }
    }
    
    // Sort by distance
    std::sort(hits.begin(), hits.end(), 
              [](const RaycastHit& a, const RaycastHit& b) { return a.distance < b.distance; });
    
    return !hits.empty();
}

void PhysicsWorld::queryAABB(const AABB& aabb, std::vector<RigidBody*>& results, CollisionLayer mask) const {
    results.clear();
    
    std::vector<RigidBody*> candidates;
    broadphase.query(aabb, candidates);
    
    for (RigidBody* body : candidates) {
        if (hasLayer(mask, body->getCollisionLayer())) {
            results.push_back(body);
        }
    }
}

void PhysicsWorld::querySphere(const glm::vec3& center, float radius, 
                                std::vector<RigidBody*>& results, CollisionLayer mask) const {
    results.clear();
    
    AABB sphereAABB = AABB::fromCenterExtents(center, glm::vec3(radius));
    std::vector<RigidBody*> candidates;
    broadphase.query(sphereAABB, candidates);
    
    for (RigidBody* body : candidates) {
        if (!hasLayer(mask, body->getCollisionLayer())) {
            continue;
        }
        
        // Fine-grained sphere test
        AABB bodyAABB = body->getWorldAABB();
        
        // Check sphere vs AABB
        glm::vec3 closest;
        closest.x = std::clamp(center.x, bodyAABB.min.x, bodyAABB.max.x);
        closest.y = std::clamp(center.y, bodyAABB.min.y, bodyAABB.max.y);
        closest.z = std::clamp(center.z, bodyAABB.min.z, bodyAABB.max.z);
        
        float distSq = glm::dot(center - closest, center - closest);
        if (distSq <= radius * radius) {
            results.push_back(body);
        }
    }
}

bool PhysicsWorld::collideWithWorld(RigidBody* body, std::vector<Contact>& contacts) const {
    if (!blockQuery) return false;
    
    AABB bodyAABB = body->getWorldAABB();
    
    // Expand slightly for safety
    bodyAABB = bodyAABB.expanded(0.1f);
    
    glm::ivec3 minBlock = glm::ivec3(glm::floor(bodyAABB.min));
    glm::ivec3 maxBlock = glm::ivec3(glm::floor(bodyAABB.max));
    
    bool anyContact = false;
    
    for (int x = minBlock.x; x <= maxBlock.x; x++) {
        for (int y = minBlock.y; y <= maxBlock.y; y++) {
            for (int z = minBlock.z; z <= maxBlock.z; z++) {
                if (blockQuery(x, y, z)) {
                    AABB blockAABB(
                        glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)),
                        glm::vec3(static_cast<float>(x + 1), static_cast<float>(y + 1), static_cast<float>(z + 1))
                    );
                    
                    Contact contact;
                    if (CollisionDetection::aabbAABB(body->getWorldAABB(), blockAABB, contact)) {
                        contact.bodyA = body;
                        contact.bodyB = nullptr; // World
                        contacts.push_back(contact);
                        anyContact = true;
                    }
                }
            }
        }
    }
    
    return anyContact;
}

} // namespace Physics
