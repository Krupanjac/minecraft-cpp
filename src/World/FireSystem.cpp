#include "FireSystem.h"
#include "ChunkManager.h"
#include "Block.h"
#include "../Render/ExplosionVolumeSystem.h"
#include "../Audio/AudioManager.h"

#include <algorithm>

FireSystem::FireSystem()
    : rng(std::random_device{}())
{
}

bool FireSystem::isBurning(const glm::ivec3& pos) const {
    return index.find(pos) != index.end();
}

void FireSystem::getFireLightPositions(std::vector<glm::vec3>& outPositions, size_t maxCount) const {
    outPositions.clear();
    if (burning.empty() || maxCount == 0) return;

    size_t count = std::min(maxCount, burning.size());
    outPositions.reserve(count);
    for (size_t i = 0; i < burning.size() && outPositions.size() < count; ++i) {
        const auto& b = burning[i];
        outPositions.push_back(glm::vec3(b.pos) + glm::vec3(0.5f, 0.8f, 0.5f));
    }
}

bool FireSystem::isFlammableBlock(const Block& block) const {
    return block.isFlammable();
}

void FireSystem::igniteBlock(const glm::ivec3& pos, float durationSeconds, bool consumes, bool canSpread) {
    auto it = index.find(pos);
    if (it != index.end()) {
        auto& existing = burning[it->second];
        float newDuration = std::max(existing.duration, durationSeconds);
        if (newDuration > existing.duration + 0.1f) {
            existing.vfxSpawned = false; // refresh VFX if fire duration extended
        }
        existing.duration = newDuration;
        existing.consumes = existing.consumes || consumes;
        existing.canSpread = existing.canSpread || canSpread;
        return;
    }

    BurningBlock b;
    b.pos = pos;
    b.duration = std::max(1.0f, durationSeconds);
    b.consumes = consumes;
    b.canSpread = canSpread;
    b.vfxSpawned = false;
    burning.push_back(b);
    index[pos] = burning.size() - 1;
}

void FireSystem::removeAtIndex(size_t i) {
    if (i >= burning.size()) return;

    glm::ivec3 removedPos = burning[i].pos;
    index.erase(removedPos);

    if (i != burning.size() - 1) {
        burning[i] = burning.back();
        index[burning[i].pos] = i;
    }
    burning.pop_back();
}

void FireSystem::update(float deltaTime, ChunkManager& chunkManager, ExplosionVolumeSystem* vfx) {
    if (burning.empty()) {
        if (fireLoopHandle != 0) {
            Audio::AudioManager::instance().fadeOutSound(fireLoopHandle, 0.6f);
            fireLoopHandle = 0;
        }
        return;
    }

    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    int vfxSpawned = 0;
    int spreadsThisFrame = 0;
    
    // Check if we're at the global VFX cap
    size_t activeFireVfx = vfx ? vfx->getActiveFireCount() : 0;
    bool canSpawnVfx = (activeFireVfx < static_cast<size_t>(maxActiveFireVfx));

    for (size_t i = 0; i < burning.size(); ) {
        auto& b = burning[i];
        b.age += deltaTime;
        b.spreadTimer += deltaTime;
        b.vfxTimer += deltaTime;
        b.vfxActiveTime += deltaTime;

        // Only spawn VFX if under global cap and per-frame budget
        bool needsVfx = !b.vfxSpawned;
        if (b.vfxSpawned && b.vfxDuration > 0.0f) {
            float refreshAt = b.vfxDuration * 0.85f;
            if (b.vfxActiveTime >= refreshAt) {
                needsVfx = true;
            }
        }

        const int vfxPerBlock = 5;
        if (vfx && canSpawnVfx && needsVfx && b.vfxTimer >= fireVfxInterval &&
            (vfxSpawned + vfxPerBlock) <= maxVfxPerUpdate) {
            float remaining = std::max(1.2f, b.duration - b.age);
            float vfxDuration = std::max(2.4f, remaining);

            // Multi-sided fire VFX around the block (top + 4 sides)
            glm::vec3 base = glm::vec3(b.pos);
            const glm::vec3 offsets[] = {
                {0.5f, 0.85f, 0.5f},  // top
                {0.1f, 0.45f, 0.5f},  // west
                {0.9f, 0.45f, 0.5f},  // east
                {0.5f, 0.45f, 0.1f},  // north
                {0.5f, 0.45f, 0.9f}   // south
            };

            const float radii[] = {0.9f, 0.75f, 0.75f, 0.75f, 0.75f};

            for (int si = 0; si < 5; ++si) {
                vfx->spawnFire(base + offsets[si], radii[si], vfxDuration);
                vfxSpawned++;
            }

            b.vfxTimer = 0.0f;
            b.vfxSpawned = true;
            b.vfxActiveTime = 0.0f;
            b.vfxDuration = vfxDuration;
        }

        if (b.canSpread && b.spreadTimer >= spreadInterval && spreadsThisFrame < maxSpreadsPerUpdate) {
            b.spreadTimer = 0.0f;

            static const glm::ivec3 dirs[6] = {
                {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
            };

            for (const auto& d : dirs) {
                glm::ivec3 npos = b.pos + d;
                if (isBurning(npos)) continue;

                Block neighbor = chunkManager.getBlockAt(npos.x, npos.y, npos.z);
                bool canBurn = isFlammableBlock(neighbor);
                bool canSpreadTo = canBurn && (neighbor.getType() != BlockType::GRASS);
                if (canSpreadTo && chanceDist(rng) < spreadChance) {
                    igniteBlock(npos, b.duration, true, true);
                    spreadsThisFrame++;
                    if (spreadsThisFrame >= maxSpreadsPerUpdate) break;
                }
            }
        }

        if (b.age >= b.duration) {
            Block block = chunkManager.getBlockAt(b.pos.x, b.pos.y, b.pos.z);
            if (b.consumes && isFlammableBlock(block)) {
                if (block.getType() == BlockType::GRASS) {
                    chunkManager.setBlockAt(b.pos.x, b.pos.y, b.pos.z, Block(BlockType::DIRT));
                } else {
                    chunkManager.setBlockAt(b.pos.x, b.pos.y, b.pos.z, Block(BlockType::AIR));
                }
            }
            removeAtIndex(i);
            continue;
        }

        ++i;
    }

    // Loop fire sound while any blocks are burning
    if (!burning.empty()) {
        glm::vec3 firePos = glm::vec3(burning.front().pos) + glm::vec3(0.5f, 0.8f, 0.5f);
        if (fireLoopHandle == 0) {
            fireLoopHandle = Audio::AudioManager::instance().playSoundAtWithRange(
                Audio::SoundType::FIRE, firePos, 0.7f, 24.0f, 1.0f);
            Audio::AudioManager::instance().setLoopRegion(fireLoopHandle, 0.25f, 0.85f);
        } else {
            if (!Audio::AudioManager::instance().setSoundPosition(fireLoopHandle, firePos)) {
                fireLoopHandle = 0;
            }
        }
    } else if (fireLoopHandle != 0) {
        Audio::AudioManager::instance().fadeOutSound(fireLoopHandle, 0.6f);
        fireLoopHandle = 0;
    }
}
