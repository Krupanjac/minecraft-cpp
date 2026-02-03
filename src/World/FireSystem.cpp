#include "FireSystem.h"
#include "ChunkManager.h"
#include "Block.h"
#include "../Render/ExplosionVolumeSystem.h"

#include <algorithm>

FireSystem::FireSystem()
    : rng(std::random_device{}())
{
}

bool FireSystem::isBurning(const glm::ivec3& pos) const {
    return index.find(pos) != index.end();
}

bool FireSystem::isFlammableBlock(const Block& block) const {
    // Organic materials that can burn
    if (block.isLeaves() || block.isLog() || block.isPlanks()) return true;
    
    // Grass block (the grass-topped dirt), plants and flowers
    BlockType t = block.getType();
    return t == BlockType::GRASS ||
           t == BlockType::TALL_GRASS ||
           t == BlockType::ROSE ||
           t == BlockType::COBWEB ||       // Spider webs burn fast
           t == BlockType::BOOKSHELF;      // Paper burns
}

void FireSystem::igniteBlock(const glm::ivec3& pos, float durationSeconds, bool consumes, bool canSpread) {
    auto it = index.find(pos);
    if (it != index.end()) {
        auto& existing = burning[it->second];
        existing.duration = std::max(existing.duration, durationSeconds);
        existing.consumes = existing.consumes || consumes;
        existing.canSpread = existing.canSpread || canSpread;
        return;
    }

    BurningBlock b;
    b.pos = pos;
    b.duration = std::max(1.0f, durationSeconds);
    b.consumes = consumes;
    b.canSpread = canSpread;
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
    if (burning.empty()) return;

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

        // Only spawn VFX if under global cap and per-frame budget
        if (vfx && canSpawnVfx && b.vfxTimer >= fireVfxInterval && vfxSpawned < maxVfxPerUpdate) {
            glm::vec3 center = glm::vec3(b.pos) + glm::vec3(0.5f, 0.25f, 0.5f);
            // Smaller fire VFX (0.7 radius instead of 0.9) for better performance
            vfx->spawnFire(center, 0.7f, 1.0f);
            b.vfxTimer = 0.0f;
            vfxSpawned++;
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
                if (isFlammableBlock(neighbor) && chanceDist(rng) < spreadChance) {
                    igniteBlock(npos, b.duration, true, true);
                    spreadsThisFrame++;
                    if (spreadsThisFrame >= maxSpreadsPerUpdate) break;
                }
            }
        }

        if (b.age >= b.duration) {
            Block block = chunkManager.getBlockAt(b.pos.x, b.pos.y, b.pos.z);
            if (b.consumes && isFlammableBlock(block)) {
                chunkManager.setBlockAt(b.pos.x, b.pos.y, b.pos.z, Block(BlockType::AIR));
            }
            removeAtIndex(i);
            continue;
        }

        ++i;
    }
}
