#include "RigidBody.h"
#include "Collider.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace Physics {

uint32_t RigidBody::nextId = 1;

RigidBody::RigidBody()
    : position(0.0f)
    , rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    , linearVelocity(0.0f)
    , angularVelocity(0.0f)
    , accumulatedForce(0.0f)
    , accumulatedTorque(0.0f)
    , mass(1.0f)
    , inverseMass(1.0f)
    , inertia(glm::mat3(1.0f))
    , inverseInertia(glm::mat3(1.0f))
    , type(Type::Dynamic)
    , collisionLayer(CollisionLayer::Default)
    , collisionMask(CollisionLayer::All)
    , awake(true)
    , canSleepFlag(true)
    , sleepTimer(0.0f)
    , gravityScale(1.0f)
    , useGravity(true)
    , userData(nullptr)
    , id(nextId++)
    , deltaLinearVelocity(0.0f)
    , deltaAngularVelocity(0.0f)
{
}

RigidBody::RigidBody(Type type)
    : RigidBody()
{
    setType(type);
}

void RigidBody::setPosition(const glm::vec3& pos) {
    position = pos;
    setAwake(true);
}

void RigidBody::setRotation(const glm::quat& rot) {
    rotation = glm::normalize(rot);
    setAwake(true);
}

void RigidBody::setTransform(const glm::vec3& pos, const glm::quat& rot) {
    position = pos;
    rotation = glm::normalize(rot);
    setAwake(true);
}

glm::mat4 RigidBody::getTransformMatrix() const {
    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, position);
    m *= glm::toMat4(rotation);
    return m;
}

void RigidBody::setLinearVelocity(const glm::vec3& vel) {
    if (type == Type::Static) return;
    linearVelocity = vel;
    setAwake(true);
}

void RigidBody::setAngularVelocity(const glm::vec3& vel) {
    if (type == Type::Static) return;
    angularVelocity = vel;
    setAwake(true);
}

glm::vec3 RigidBody::getVelocityAtPoint(const glm::vec3& worldPoint) const {
    glm::vec3 r = worldPoint - position;
    return linearVelocity + glm::cross(angularVelocity, r);
}

void RigidBody::applyForce(const glm::vec3& force) {
    if (type != Type::Dynamic) return;
    accumulatedForce += force;
    setAwake(true);
}

void RigidBody::applyForceAtPoint(const glm::vec3& force, const glm::vec3& worldPoint) {
    if (type != Type::Dynamic) return;
    accumulatedForce += force;
    glm::vec3 r = worldPoint - position;
    accumulatedTorque += glm::cross(r, force);
    setAwake(true);
}

void RigidBody::applyTorque(const glm::vec3& torque) {
    if (type != Type::Dynamic) return;
    accumulatedTorque += torque;
    setAwake(true);
}

void RigidBody::applyImpulse(const glm::vec3& impulse) {
    if (type != Type::Dynamic) return;
    linearVelocity += impulse * inverseMass;
    setAwake(true);
}

void RigidBody::applyImpulseAtPoint(const glm::vec3& impulse, const glm::vec3& worldPoint) {
    if (type != Type::Dynamic) return;
    
    linearVelocity += impulse * inverseMass;
    
    glm::vec3 r = worldPoint - position;
    glm::vec3 angularImpulse = glm::cross(r, impulse);
    angularVelocity += getWorldInverseInertia() * angularImpulse;
    
    setAwake(true);
}

void RigidBody::clearForces() {
    accumulatedForce = glm::vec3(0.0f);
    accumulatedTorque = glm::vec3(0.0f);
}

void RigidBody::setMass(float m) {
    if (type == Type::Static || m <= 0.0f) {
        mass = 0.0f;
        inverseMass = 0.0f;
    } else {
        mass = m;
        inverseMass = 1.0f / m;
    }
}

void RigidBody::setInertia(const glm::mat3& i) {
    inertia = i;
    
    // Compute inverse (handle static case)
    if (type == Type::Static) {
        inverseInertia = glm::mat3(0.0f);
    } else {
        float det = glm::determinant(inertia);
        if (std::abs(det) > 1e-6f) {
            inverseInertia = glm::inverse(inertia);
        } else {
            inverseInertia = glm::mat3(0.0f);
        }
    }
}

glm::mat3 RigidBody::getWorldInverseInertia() const {
    if (type == Type::Static) {
        return glm::mat3(0.0f);
    }
    
    glm::mat3 rotMatrix = glm::toMat3(rotation);
    return rotMatrix * inverseInertia * glm::transpose(rotMatrix);
}

void RigidBody::setType(Type t) {
    type = t;
    
    if (type == Type::Static) {
        linearVelocity = glm::vec3(0.0f);
        angularVelocity = glm::vec3(0.0f);
        inverseMass = 0.0f;
        inverseInertia = glm::mat3(0.0f);
    } else if (inverseMass == 0.0f && mass > 0.0f) {
        inverseMass = 1.0f / mass;
        // Recalculate inverse inertia
        float det = glm::determinant(inertia);
        if (std::abs(det) > 1e-6f) {
            inverseInertia = glm::inverse(inertia);
        }
    }
}

bool RigidBody::canCollideWith(const RigidBody& other) const {
    // Check layer masks
    bool layerMatch = hasLayer(collisionMask, other.collisionLayer) &&
                      hasLayer(other.collisionMask, collisionLayer);
    
    // Two static/kinematic bodies don't need to collide with each other
    if (!isDynamic() && !other.isDynamic()) {
        return false;
    }
    
    return layerMatch;
}

void RigidBody::setAwake(bool wake) {
    if (wake) {
        awake = true;
        sleepTimer = 0.0f;
    } else if (canSleepFlag) {
        awake = false;
        linearVelocity = glm::vec3(0.0f);
        angularVelocity = glm::vec3(0.0f);
    }
}

void RigidBody::integrateForces(float dt, const glm::vec3& gravity) {
    if (type != Type::Dynamic || !awake) return;
    
    // Apply gravity
    if (useGravity) {
        linearVelocity += gravity * gravityScale * dt;
    }
    
    // Apply accumulated forces
    linearVelocity += accumulatedForce * inverseMass * dt;
    angularVelocity += getWorldInverseInertia() * accumulatedTorque * dt;
    
    // Apply damping
    linearVelocity *= std::pow(1.0f - material.linearDamping, dt);
    angularVelocity *= std::pow(1.0f - material.angularDamping, dt);
    
    clearForces();
}

void RigidBody::integrateVelocity(float dt) {
    if (type == Type::Static || !awake) return;
    
    // Apply solver deltas
    linearVelocity += deltaLinearVelocity;
    angularVelocity += deltaAngularVelocity;
    deltaLinearVelocity = glm::vec3(0.0f);
    deltaAngularVelocity = glm::vec3(0.0f);
    
    // Integrate position
    position += linearVelocity * dt;
    
    // Integrate rotation (using quaternion derivative)
    if (glm::length(angularVelocity) > 1e-6f) {
        glm::quat angVelQuat(0.0f, angularVelocity.x, angularVelocity.y, angularVelocity.z);
        rotation += (angVelQuat * rotation) * (0.5f * dt);
        rotation = glm::normalize(rotation);
    }
}

void RigidBody::updateSleepState(float dt, float linearThreshold, float angularThreshold, float timeThreshold) {
    if (type != Type::Dynamic || !canSleepFlag) return;
    
    float linearSpeed = glm::length(linearVelocity);
    float angularSpeed = glm::length(angularVelocity);
    
    if (linearSpeed < linearThreshold && angularSpeed < angularThreshold) {
        sleepTimer += dt;
        if (sleepTimer >= timeThreshold) {
            setAwake(false);
        }
    } else {
        sleepTimer = 0.0f;
    }
}

AABB RigidBody::getWorldAABB() const {
    if (collider) {
        return collider->getWorldAABB(getTransformMatrix());
    }
    
    // Default small AABB around position
    return AABB::fromCenterExtents(position, glm::vec3(0.1f));
}

} // namespace Physics
