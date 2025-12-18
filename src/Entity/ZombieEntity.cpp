#include "ZombieEntity.h"

#include "../Core/Logger.h"
#include "../Model/Model.h"
#include "../World/ChunkManager.h"

#include <algorithm>
#include <cctype>
#include <queue>
#include <unordered_map>
#include <unordered_set>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string pickAnimByKeywords(const std::vector<std::string>& names, std::initializer_list<const char*> keys) {
    for (const auto& n : names) {
        std::string ln = toLower(n);
        for (auto* k : keys) {
            if (ln.find(k) != std::string::npos) return n;
        }
    }
    return "";
}

static bool checkMobCollision(ChunkManager& chunkManager, const glm::vec3& feetPos) {
    // AABB approx of a Minecraft mob. feetPos is at feet.
    constexpr float HALF_W = 0.30f;
    constexpr float HEIGHT = 1.80f;

    float minX = feetPos.x - HALF_W;
    float maxX = feetPos.x + HALF_W;
    float minY = feetPos.y;
    float maxY = feetPos.y + HEIGHT;
    float minZ = feetPos.z - HALF_W;
    float maxZ = feetPos.z + HALF_W;

    for (int x = (int)std::floor(minX); x <= (int)std::floor(maxX); ++x) {
        for (int y = (int)std::floor(minY); y <= (int)std::floor(maxY); ++y) {
            for (int z = (int)std::floor(minZ); z <= (int)std::floor(maxZ); ++z) {
                Block b = chunkManager.getBlockAt(x, y, z);
                if (b.isSolid()) return true;
            }
        }
    }
    return false;
}

static std::string pickWalkAnimPreferWalk(const std::vector<std::string>& names) {
    // Prefer true walk over run (user request)
    std::string w = pickAnimByKeywords(names, {"walk"});
    if (!w.empty()) return w;
    w = pickAnimByKeywords(names, {"run"});
    if (!w.empty()) return w;
    return pickAnimByKeywords(names, {"move"});
}

static std::string pickIdlePreferIdle1(const std::vector<std::string>& names) {
    // User wants idle1 specifically (if present)
    std::string a = pickAnimByKeywords(names, {"idle1"});
    if (!a.empty()) return a;
    a = pickAnimByKeywords(names, {"idle"});
    if (!a.empty()) return a;
    // Avoid "stand" unless it's the only thing available
    a = pickAnimByKeywords(names, {"stand"});
    if (!a.empty()) return a;
    return names.empty() ? "" : names[0];
}

ZombieEntity::ZombieEntity(const glm::vec3& startPos) : Entity(startPos) {
    std::string modelPath = "assets/models/Zombie/scene.gltf";
    auto zombieModel = std::make_shared<ModelSystem::Model>(modelPath);
    setModel(zombieModel);

    // Match player scale
    setScale(glm::vec3(0.03f));

    // Axis fix for this Zombie asset (upright) + yaw flip (model forward is reversed).
    rotationOffset = glm::vec3(90.0f, 180.0f, 0.0f);
    setRotation(rotationOffset);

    // Stable-ish seed from position
    unsigned int seed = 1337u
        ^ (unsigned int)(std::abs((int)startPos.x) * 73856093)
        ^ (unsigned int)(std::abs((int)startPos.z) * 19349663);
    rng.seed(seed);

    pickAnimations();
    setState(State::Idle, 0.5f, 2.0f);
}

void ZombieEntity::pickAnimations() {
    if (!model) return;
    auto names = model->getAnimationNames();
    if (names.empty()) {
        LOG_WARNING("Zombie: no animations found in glTF");
        return;
    }

    // Try to find best matches in the Zombie glTF
    idleAnim = pickIdlePreferIdle1(names);
    walkAnim = pickWalkAnimPreferWalk(names);

    // Fallbacks
    if (idleAnim.empty()) idleAnim = names[0];
    if (walkAnim.empty()) walkAnim = (names.size() > 1) ? names[1] : names[0];

    LOG_INFO("Zombie animations: idle='" + idleAnim + "' walk='" + walkAnim + "'");

    model->playAnimation(idleAnim, true);
}

void ZombieEntity::setState(State s, float minTime, float maxTime) {
    state = s;
    std::uniform_real_distribution<float> dis(minTime, maxTime);
    stateTimer = dis(rng);
    if (state == State::Wander) chooseRandomWanderDir();
}

void ZombieEntity::chooseRandomWanderDir() {
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    float a = dis(rng) * 6.2831853f;
    desiredDir = glm::normalize(glm::vec3(std::cos(a), 0.0f, std::sin(a)));
}

glm::vec3 ZombieEntity::consumeAttackImpulse() {
    glm::vec3 out = attackImpulse;
    attackImpulse = glm::vec3(0.0f);
    return out;
}

static bool tryStepUp(ChunkManager& chunkManager, glm::vec3& pos, float dx, float dz) {
    // Minecraft-ish step-up: try moving up 1 block to clear a small ledge.
    constexpr float STEP = 1.0f;
    glm::vec3 tryPos = pos;
    tryPos.y += STEP;
    if (checkMobCollision(chunkManager, tryPos)) return false;
    tryPos.x += dx;
    tryPos.z += dz;
    if (checkMobCollision(chunkManager, tryPos)) return false;
    pos = tryPos;
    return true;
}

struct GridKey {
    int x, z;
    bool operator==(const GridKey& o) const { return x == o.x && z == o.z; }
};
struct GridKeyHash {
    size_t operator()(const GridKey& k) const noexcept {
        // cheap hash
        return (size_t)(k.x * 73856093) ^ (size_t)(k.z * 19349663);
    }
};

static bool isWalkable(ChunkManager& cm, int x, int yFeet, int z) {
    // Need solid below + 2-block clearance.
    Block below = cm.getBlockAt(x, yFeet - 1, z);
    if (!below.isSolid() && !below.isWater()) return false;
    Block feet = cm.getBlockAt(x, yFeet, z);
    Block head = cm.getBlockAt(x, yFeet + 1, z);
    return !feet.isSolid() && !head.isSolid();
}

static int findWalkableY(ChunkManager& cm, int x, int z, int yHint) {
    // Search around yHint for a walkable spot (step up/down 1).
    for (int dy = 0; dy <= 1; ++dy) {
        int y = yHint + dy;
        if (isWalkable(cm, x, y, z)) return y;
    }
    for (int dy = 1; dy <= 2; ++dy) {
        int y = yHint - dy;
        if (isWalkable(cm, x, y, z)) return y;
    }
    return INT32_MIN;
}

static std::vector<glm::vec3> findPathAStar(ChunkManager& cm, const glm::vec3& startFeet, const glm::vec3& goalFeet, int maxRadius, int maxIters) {
    // 2D A* in x/z; y is resolved per-node via findWalkableY().
    int sx = (int)std::floor(startFeet.x);
    int sz = (int)std::floor(startFeet.z);
    int sy = (int)std::floor(startFeet.y);

    int gx = (int)std::floor(goalFeet.x);
    int gz = (int)std::floor(goalFeet.z);
    int gy = (int)std::floor(goalFeet.y);

    int startY = findWalkableY(cm, sx, sz, sy);
    if (startY == INT32_MIN) return {};
    int goalY = findWalkableY(cm, gx, gz, gy);
    if (goalY == INT32_MIN) {
        // If player spot isn't walkable, still path toward closest x/z
        goalY = gy;
    }

    struct NodeRec {
        GridKey k;
        int yFeet;
        float g;
        float f;
    };
    struct Cmp {
        bool operator()(const NodeRec& a, const NodeRec& b) const { return a.f > b.f; }
    };

    auto h = [&](int x, int z) {
        return (float)(std::abs(x - gx) + std::abs(z - gz));
    };

    std::priority_queue<NodeRec, std::vector<NodeRec>, Cmp> open;
    std::unordered_map<GridKey, float, GridKeyHash> bestG;
    std::unordered_map<GridKey, GridKey, GridKeyHash> cameFrom;
    std::unordered_map<GridKey, int, GridKeyHash> yAt;

    GridKey startK{sx, sz};
    bestG[startK] = 0.0f;
    yAt[startK] = startY;
    open.push(NodeRec{startK, startY, 0.0f, h(sx, sz)});

    auto inBounds = [&](int x, int z) {
        return std::abs(x - sx) <= maxRadius && std::abs(z - sz) <= maxRadius;
    };

    GridKey bestGoal = startK;
    float bestGoalH = h(sx, sz);

    int iters = 0;
    while (!open.empty() && iters++ < maxIters) {
        NodeRec cur = open.top();
        open.pop();

        // Stale check
        auto itG = bestG.find(cur.k);
        if (itG == bestG.end() || cur.g > itG->second + 1e-4f) continue;

        float curH = h(cur.k.x, cur.k.z);
        if (curH < bestGoalH) { bestGoalH = curH; bestGoal = cur.k; }

        if (cur.k.x == gx && cur.k.z == gz && std::abs(cur.yFeet - goalY) <= 1) {
            bestGoal = cur.k;
            break;
        }

        static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (auto& d : dirs) {
            int nx = cur.k.x + d[0];
            int nz = cur.k.z + d[1];
            if (!inBounds(nx, nz)) continue;

            int ny = findWalkableY(cm, nx, nz, cur.yFeet);
            if (ny == INT32_MIN) continue;

            float stepCost = 1.0f + 0.5f * (float)std::abs(ny - cur.yFeet); // prefer flat
            float ng = cur.g + stepCost;

            GridKey nk{nx, nz};
            auto bg = bestG.find(nk);
            if (bg == bestG.end() || ng < bg->second) {
                bestG[nk] = ng;
                cameFrom[nk] = cur.k;
                yAt[nk] = ny;
                open.push(NodeRec{nk, ny, ng, ng + h(nx, nz)});
            }
        }
    }

    // Reconstruct from bestGoal
    std::vector<glm::vec3> out;
    GridKey cur = bestGoal;
    int y = yAt[cur];
    out.push_back(glm::vec3((float)cur.x + 0.5f, (float)y, (float)cur.z + 0.5f));
    while (!(cur.x == sx && cur.z == sz)) {
        auto it = cameFrom.find(cur);
        if (it == cameFrom.end()) break;
        cur = it->second;
        y = yAt[cur];
        out.push_back(glm::vec3((float)cur.x + 0.5f, (float)y, (float)cur.z + 0.5f));
    }
    std::reverse(out.begin(), out.end());
    return out;
}

bool ZombieEntity::updateAI(float deltaTime, ChunkManager& chunkManager, const glm::vec3& playerPos) {
    // Track previous transform for motion vectors (TAA stability)
    prevPosition = position;
    prevRotation = rotation;
    prevScale = scale;

    attackImpulse = glm::vec3(0.0f);
    bool attacked = false;

    attackCooldown = std::max(0.0f, attackCooldown - deltaTime);
    stateTimer -= deltaTime;

    // Horizontal distance to player (feet-to-feet)
    glm::vec3 toPlayer = playerPos - position;
    float distXZ = glm::length(glm::vec2(toPlayer.x, toPlayer.z));

    constexpr float CHASE_RANGE = 18.0f;
    constexpr float GIVE_UP_RANGE = 24.0f;
    constexpr float ATTACK_RANGE = 1.6f;

    // Simple state transitions
    if (state != State::Chase && distXZ < CHASE_RANGE) {
        state = State::Chase;
        stateTimer = 9999.0f;
    } else if (state == State::Chase && distXZ > GIVE_UP_RANGE) {
        setState(State::Idle, 0.5f, 2.0f);
    } else if (state != State::Chase && stateTimer <= 0.0f) {
        // Idle/Wander loop
        if (state == State::Idle) setState(State::Wander, 1.5f, 4.0f);
        else setState(State::Idle, 0.8f, 2.5f);
    }

    // Desired move direction + speed
    glm::vec3 dir(0.0f);
    float speed = 0.0f;

    if (state == State::Chase) {
        // Movement speed tuned to match animation cadence better (less "sliding")
        speed = 1.25f;

        // Replan periodically (and when we have no path)
        pathReplanTimer -= deltaTime;
        if (pathReplanTimer <= 0.0f || pathPoints.empty() || pathIndex >= pathPoints.size()) {
            pathReplanTimer = 0.6f;
            pathPoints = findPathAStar(chunkManager, position, playerPos, /*maxRadius*/24, /*maxIters*/2500);
            pathIndex = 0;
        }

        // Follow path points
        glm::vec3 target = playerPos;
        if (!pathPoints.empty() && pathIndex < pathPoints.size()) {
            target = pathPoints[pathIndex];
            float d = glm::length(glm::vec2(target.x - position.x, target.z - position.z));
            if (d < 0.8f && pathIndex + 1 < pathPoints.size()) pathIndex++;
        }

        glm::vec3 toT = target - position;
        if (glm::length(glm::vec2(toT.x, toT.z)) > 0.001f) dir = glm::normalize(glm::vec3(toT.x, 0.0f, toT.z));
    } else if (state == State::Wander) {
        dir = desiredDir;
        speed = 0.9f;
    } else {
        speed = 0.0f;
    }

    // Face movement direction
    if (glm::length(glm::vec2(dir.x, dir.z)) > 0.001f) {
        // Force facing the PLAYER while chasing (user request), otherwise face movement.
        glm::vec3 faceDir = dir;
        if (state == State::Chase && distXZ > 0.001f) {
            faceDir = glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
        }

        float yaw = std::atan2(-faceDir.x, -faceDir.z);
        rotation.x = rotationOffset.x;
        rotation.y = glm::degrees(yaw) + rotationOffset.y;
        rotation.z = rotationOffset.z;
    }
    else {
        // Ensure we keep the axis fix even while idle
        rotation.x = rotationOffset.x;
        rotation.z = rotationOffset.z;
    }

    // If we spawned inside blocks (or chunk just loaded), push up until not colliding.
    // This uses the same AABB collision as movement.
    for (int i = 0; i < 8; ++i) {
        if (!checkMobCollision(chunkManager, position)) break;
        position.y += 1.0f;
        velocity.y = 0.0f;
    }

    // Extra: if we're still colliding, try a small local nudge in XZ to escape corners.
    if (checkMobCollision(chunkManager, position)) {
        static const glm::vec3 nudges[] = {
            {0.5f, 0.0f, 0.0f}, {-0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.5f}, {0.0f, 0.0f, -0.5f}
        };
        for (const auto& n : nudges) {
            glm::vec3 p = position + n;
            if (!checkMobCollision(chunkManager, p)) { position = p; break; }
        }
    }

    // Very simple "attack": apply knockback impulse when close enough
    if (state == State::Chase && distXZ < ATTACK_RANGE && attackCooldown <= 0.0f) {
        glm::vec3 away = (distXZ > 0.001f) ? glm::normalize(glm::vec3(-toPlayer.x, 0.0f, -toPlayer.z)) : glm::vec3(0.0f, 0.0f, 1.0f);
        attackImpulse = away * 3.5f + glm::vec3(0.0f, 2.0f, 0.0f);
        attackCooldown = 1.2f;
        attacked = true;
    }

    // === Player-like physics & collision ===
    // position is FEET position for zombies.
    velocity.x = dir.x * speed;
    velocity.z = dir.z * speed;

    // Water check (feet + head)
    bool inWater = false;
    {
        int fx = (int)std::floor(position.x);
        int fz = (int)std::floor(position.z);
        Block feet = chunkManager.getBlockAt(fx, (int)std::floor(position.y + 0.1f), fz);
        Block head = chunkManager.getBlockAt(fx, (int)std::floor(position.y + 1.6f), fz);
        inWater = feet.isWater() || head.isWater();
    }

    if (inWater) {
        float drag = 1.0f - (2.0f * deltaTime);
        drag = std::max(0.0f, drag);
        velocity.x *= drag;
        velocity.z *= drag;
        velocity.y *= drag;

        velocity.y -= 2.0f * deltaTime;
        velocity.y = std::max(-4.0f, std::min(4.0f, velocity.y));
    } else {
        velocity.y -= 32.0f * deltaTime;
        velocity.y = std::max(-78.4f, velocity.y);
    }

    glm::vec3 pos = position;
    glm::vec3 step = velocity * deltaTime;

    // X
    if (checkMobCollision(chunkManager, glm::vec3(pos.x + step.x, pos.y, pos.z))) {
        // If grounded, try stepping up 1 block before giving up.
        if (!(onGround && tryStepUp(chunkManager, pos, step.x, 0.0f))) {
            step.x = 0.0f;
            velocity.x = 0.0f;
            if (state != State::Chase) chooseRandomWanderDir();
        }
    } else {
        pos.x += step.x;
    }

    // Z
    if (checkMobCollision(chunkManager, glm::vec3(pos.x, pos.y, pos.z + step.z))) {
        if (!(onGround && tryStepUp(chunkManager, pos, 0.0f, step.z))) {
            step.z = 0.0f;
            velocity.z = 0.0f;
            if (state != State::Chase) chooseRandomWanderDir();
        }
    } else {
        pos.z += step.z;
    }

    // Y
    if (checkMobCollision(chunkManager, glm::vec3(pos.x, pos.y + step.y, pos.z))) {
        if (step.y < 0.0f) onGround = true;
        step.y = 0.0f;
        velocity.y = 0.0f;
    } else {
        onGround = false;
    }
    pos.y += step.y;

    position = pos;

    // Animation switching (reuse same logic style as PlayerEntity)
    if (model) {
        float horizSpeed = glm::length(glm::vec2(dir.x, dir.z)) * speed;
        std::string currentAnim = model->getCurrentAnimation();
        if (horizSpeed > 0.05f) {
            // Cut walk loop to 1/3 to avoid long forward drift in this glTF (root motion baked in).
            model->setAnimationLoopEndFactor(1.0f / 5.0f);
            // Hard fix: lock root-motion XZ so the mesh doesn't drift ahead of the entity.
            model->setLockRootMotionXZ(true);
            // Match stride cadence to actual movement speed to reduce foot sliding.
            // (Reference: ~1.25 was our chase speed; scale around that.)
            float animSpeed = std::clamp(speed / 1.25f, 0.8f, 1.0f);
            model->setAnimationSpeed(animSpeed);
            if (!walkAnim.empty() && currentAnim != walkAnim) model->playAnimation(walkAnim, true);
        } else {
            model->setAnimationLoopEndFactor(1.0f);
            model->setLockRootMotionXZ(false);
            model->setAnimationSpeed(1.0f);
            if (!idleAnim.empty() && currentAnim != idleAnim) model->playAnimation(idleAnim, true);
        }
    }

    // Update animation time (without Entity::update's position integration)
    if (model) model->updateAnimation(deltaTime);
    return attacked;
}


