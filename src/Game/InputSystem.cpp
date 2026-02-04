#define NOMINMAX
#include "InputSystem.h"

#include "../Audio/AudioManager.h"
#include "../Core/Logger.h"
#include "../Core/Window.h"
#include "../Entity/Entity.h"
#include "../Entity/EntityManager.h"
#include "../Entity/PlayerEntity.h"
#include "../Entity/ZombieEntity.h"
#include "../Entity/SkeletonEntity.h"
#include "../Entity/PigEntity.h"
#include "../Entity/ChickenEntity.h"
#include "../Entity/SheepEntity.h"
#include "../Network/NetworkManager.h"
#include "../Render/Camera.h"
#include "../Render/HeldItemRenderer.h"
#include "../UI/UIManager.h"
#include "../Util/Config.h"
#include "../World/ChunkManager.h"
#include "../World/Item.h"
#include "../Physics/PhysicsTest.h"

#include <algorithm>
#include <cmath>

InputSystem::InputSystem(UIManager& uiManagerRef,
                         Camera& cameraRef,
                         ChunkManager& chunkManagerRef,
                         HeldItemRenderer& heldItemRendererRef,
                         EntityManager& entityManagerRef,
                         Network::NetworkManager& networkManagerRef,
                         Physics::PhysicsTestSystem& physicsTestRef,
                         std::unique_ptr<PlayerEntity>& playerEntityRef,
                         std::vector<std::unique_ptr<ZombieEntity>>& zombiesRef,
                         std::vector<std::unique_ptr<SkeletonEntity>>& skeletonsRef,
                         std::vector<std::unique_ptr<PigEntity>>& pigsRef,
                         std::vector<std::unique_ptr<ChickenEntity>>& chickensRef,
                         std::vector<std::unique_ptr<SheepEntity>>& sheepRef,
                         bool& useNewEntityManagerRef,
                         float& attackCooldownRef,
                         bool& isBreakingBlockRef,
                         float& blockBreakProgressRef,
                         glm::ivec3& breakingBlockPosRef,
                         BlockType& breakingBlockTypeRef,
                         bool& isUnderwaterRef)
    : uiManager(uiManagerRef),
      camera(cameraRef),
      chunkManager(chunkManagerRef),
      heldItemRenderer(heldItemRendererRef),
      entityManager(entityManagerRef),
      networkManager(networkManagerRef),
    physicsTest(physicsTestRef),
      playerEntity(playerEntityRef),
      zombies(zombiesRef),
      skeletons(skeletonsRef),
      pigs(pigsRef),
      chickens(chickensRef),
      sheep(sheepRef),
      useNewEntityManager(useNewEntityManagerRef),
      attackCooldown(attackCooldownRef),
      isBreakingBlock(isBreakingBlockRef),
      blockBreakProgress(blockBreakProgressRef),
      breakingBlockPos(breakingBlockPosRef),
      breakingBlockType(breakingBlockTypeRef),
      isUnderwater(isUnderwaterRef) {
}

void InputSystem::setWindow(Window* windowRef) {
    window = windowRef;
}

void InputSystem::handleMouseButton(int button, int action, int /*mods*/) {
    if (uiManager.isMenuOpen()) return;

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            ItemType heldItem = uiManager.getSelectedItem();
            bool isHoldingTool = heldItem != ItemType::NONE;
            bool isSword = isHoldingTool && ItemRegistry::getCategory(heldItem) == ToolCategory::SWORD;
            (void)isSword;

            heldItemRenderer.triggerSwing();

            if (playerEntity) {
                playerEntity->playAttackAnimation();
            }

            if (networkManager.isOnline()) {
                networkManager.sendPlayerAnimation(Network::PlayerAnimationPacket::ANIM_ATTACK);
            }

            bool attackedEntity = false;
            if (attackCooldown <= 0.0f) {
                glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
                float attackRange = 4.0f;

                Entity* targetEntity = nullptr;
                float closestDist = attackRange;

                std::vector<Entity*> entities;
                if (useNewEntityManager) {
                    entities = entityManager.getAllEntities();
                } else {
                    for (auto& m : zombies) if (m) entities.push_back(m.get());
                    for (auto& m : skeletons) if (m && !m->isDead()) entities.push_back(m.get());
                    for (auto& m : pigs) if (m && !m->isDead()) entities.push_back(m.get());
                    for (auto& m : chickens) if (m && !m->isDead()) entities.push_back(m.get());
                    for (auto& m : sheep) if (m && !m->isDead()) entities.push_back(m.get());
                }

                auto remotePlayers = networkManager.getRemotePlayerEntities();
                for (auto* rp : remotePlayers) {
                    entities.push_back(rp);
                }

                for (Entity* e : entities) {
                    if (e->isDead()) continue;

                    glm::vec3 toEntity = e->getPosition() + glm::vec3(0.0f, 0.9f, 0.0f) - eyePos;
                    float dist = glm::length(toEntity);

                    if (dist < closestDist) {
                        float dot = glm::dot(glm::normalize(toEntity), camera.getFront());
                        if (dot > 0.5f) {
                            glm::vec3 entityMin = e->getPosition() - glm::vec3(0.3f, 0.0f, 0.3f);
                            glm::vec3 entityMax = e->getPosition() + glm::vec3(0.3f, 1.8f, 0.3f);

                            glm::vec3 rayDir = camera.getFront();
                            float tMin = 0.0f, tMax = attackRange;

                            for (int i = 0; i < 3; ++i) {
                                if (std::abs(rayDir[i]) < 0.0001f) {
                                    if (eyePos[i] < entityMin[i] || eyePos[i] > entityMax[i]) {
                                        tMin = attackRange + 1.0f;
                                    }
                                } else {
                                    float t1 = (entityMin[i] - eyePos[i]) / rayDir[i];
                                    float t2 = (entityMax[i] - eyePos[i]) / rayDir[i];
                                    if (t1 > t2) std::swap(t1, t2);
                                    tMin = (std::max)(tMin, t1);
                                    tMax = (std::min)(tMax, t2);
                                }
                            }

                            if (tMin <= tMax && tMin < closestDist) {
                                closestDist = tMin;
                                targetEntity = e;
                            }
                        }
                    }
                }

                if (targetEntity) {
                    attackedEntity = true;

                    float damage = ItemRegistry::instance().getAttackDamage(heldItem);
                    float knockback = ItemRegistry::instance().getKnockback(heldItem);

                    LOG_INFO("[INPUT] Attacking entity! damage=" + std::to_string(damage));

                    glm::vec3 knockbackDir = glm::normalize(targetEntity->getPosition() - camera.getPosition());
                    knockbackDir.y = 0.0f;
                    if (glm::length(knockbackDir) > 0.001f) {
                        knockbackDir = glm::normalize(knockbackDir);
                    }

                    glm::vec3 knockbackVec = knockbackDir * knockback * 10.0f;

                    Network::RemotePlayerEntity* remotePlayer = dynamic_cast<Network::RemotePlayerEntity*>(targetEntity);
                    if (remotePlayer && networkManager.isOnline()) {
                        networkManager.sendPlayerDamage(remotePlayer->getPlayerId(), damage, knockbackVec);
                        LOG_INFO("Sent damage to remote player " + std::to_string(remotePlayer->getPlayerId()) + " for " + std::to_string(damage) + " damage");
                    } else {
                        // Always call handleAttackHit for limb damage (doesn't depend on physics toggle)
                        LOG_INFO("[INPUT] Calling handleAttackHit...");
                        physicsTest.handleAttackHit(targetEntity, eyePos, camera.getFront(), damage);
                        LOG_INFO("[INPUT] handleAttackHit returned");
                        targetEntity->takeDamage(damage, knockbackVec);
                    }

                    Audio::AudioManager::instance().playSoundAt(Audio::SoundType::PLAYER_HURT, targetEntity->getPosition());

                    const auto& props = ItemRegistry::instance().getProperties(heldItem);
                    attackCooldown = 1.0f / props.attackSpeed;

                    LOG_INFO("Attacked entity for " + std::to_string(damage) + " damage");
                }
            }

            if (!attackedEntity) {
                if (uiManager.isCreativeMode) {
                    glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
                    auto result = chunkManager.rayCast(eyePos, camera.getFront(), 5.0f);
                    if (result.hit) {
                        glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
                        int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x;
                        int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y;
                        int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z;

                        Block block = chunkManager.getBlockAt(x, y, z);
                        uint8_t blockTypeVal = static_cast<uint8_t>(block.getType());

                        Audio::SoundType digSound = Audio::getDigSoundForBlock(blockTypeVal);
                        Audio::AudioManager::instance().playSoundAt(digSound, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));

                        chunkManager.setBlockAt(x, y, z, Block(BlockType::AIR));

                        if (networkManager.isOnline()) {
                            networkManager.sendBlockChange(x, y, z, static_cast<uint8_t>(BlockType::AIR));
                        }
                    }
                } else {
                    isBreakingBlock = true;
                }
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (uiManager.isSelectedSlotItem()) {
                return;
            }

            glm::vec3 eyePos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
            auto result = chunkManager.rayCast(eyePos, camera.getFront(), 5.0f);
            if (result.hit) {
                glm::vec3 chunkOrigin = ChunkManager::chunkToWorld(result.chunkPos);
                int x = static_cast<int>(chunkOrigin.x) + result.blockPos.x + result.normal.x;
                int y = static_cast<int>(chunkOrigin.y) + result.blockPos.y + result.normal.y;
                int z = static_cast<int>(chunkOrigin.z) + result.blockPos.z + result.normal.z;

                bool entityCollision = false;

                float bx1 = static_cast<float>(x);
                float by1 = static_cast<float>(y);
                float bz1 = static_cast<float>(z);
                float bx2 = bx1 + 1.0f;
                float by2 = by1 + 1.0f;
                float bz2 = bz1 + 1.0f;

                auto checkOverlap = [&](const glm::vec3& pos, float width, float height) -> bool {
                    float ex1 = pos.x - width / 2.0f;
                    float ey1 = pos.y;
                    float ez1 = pos.z - width / 2.0f;
                    float ex2 = pos.x + width / 2.0f;
                    float ey2 = pos.y + height;
                    float ez2 = pos.z + width / 2.0f;

                    return (bx1 < ex2 && bx2 > ex1) &&
                           (by1 < ey2 && by2 > ey1) &&
                           (bz1 < ez2 && bz2 > ez1);
                };

                if (checkOverlap(camera.getPosition(), 0.6f, 1.8f)) {
                    entityCollision = true;
                }

                if (!entityCollision) {
                    std::vector<Entity*> entities;
                    if (useNewEntityManager) {
                        auto managed = entityManager.getAllEntities();
                        entities.insert(entities.end(), managed.begin(), managed.end());
                    } else {
                        for (auto& m : zombies) if (m) entities.push_back(m.get());
                        for (auto& m : skeletons) if (m && !m->isDead()) entities.push_back(m.get());
                        for (auto& m : pigs) if (m && !m->isDead()) entities.push_back(m.get());
                        for (auto& m : chickens) if (m && !m->isDead()) entities.push_back(m.get());
                        for (auto& m : sheep) if (m && !m->isDead()) entities.push_back(m.get());
                    }

                    for (Entity* e : entities) {
                        if (checkOverlap(e->getPosition(), 0.6f, 1.5f)) {
                            entityCollision = true;
                            break;
                        }
                    }
                }

                if (!entityCollision) {
                    BlockType blockType = uiManager.getSelectedBlock();
                    chunkManager.setBlockAt(x, y, z, Block(blockType));

                    Audio::SoundType placeSound = Audio::getPlaceSoundForBlock(static_cast<uint8_t>(blockType));
                    Audio::AudioManager::instance().playSoundAt(placeSound, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));

                    if (networkManager.isOnline()) {
                        networkManager.sendBlockChange(x, y, z, static_cast<uint8_t>(blockType));
                    }
                }
            }
        }
    } else if (action == GLFW_RELEASE) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            isBreakingBlock = false;
            blockBreakProgress = 0.0f;
        }
    }
}

void InputSystem::processInput(float deltaTime) {
    if (!window) return;

    double xpos, ypos;
    glfwGetCursorPos(window->getNative(), &xpos, &ypos);

    int winW, winH;
    glfwGetWindowSize(window->getNative(), &winW, &winH);
    int fbW, fbH;
    glfwGetFramebufferSize(window->getNative(), &fbW, &fbH);

    double uiX = xpos;
    double uiY = ypos;

    if (winW > 0 && winH > 0) {
        uiX *= static_cast<double>(fbW) / winW;
        uiY *= static_cast<double>(fbH) / winH;
    }

    bool mousePressed = glfwGetMouseButton(window->getNative(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightMousePressed = glfwGetMouseButton(window->getNative(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (uiManager.isMenuOpen()) {
        uiManager.update(deltaTime, uiX, uiY, mousePressed, rightMousePressed);
        firstMouse = true;
        return;
    }

    const auto& keys = Settings::instance().keys;
    bool forward = window->isKeyPressed(keys.forward);
    bool backward = window->isKeyPressed(keys.backward);
    bool left = window->isKeyPressed(keys.left);
    bool right = window->isKeyPressed(keys.right);
    bool up = window->isKeyPressed(keys.jump);

    bool sprint = window->isKeyPressed(keys.sprint);
    bool sneak = window->isKeyPressed(keys.sneak);
    bool down = sneak;

    bool wasOnGroundBefore = camera.onGround;

    camera.processInput(forward, backward, left, right, up, down, sprint, sneak, deltaTime);

    if (wasOnGroundBefore && up && !camera.onGround && !camera.isFlying && uiManager.isWorldLoaded() && !isUnderwater) {
        glm::vec3 playerPos = camera.getPosition();
        int blockX = static_cast<int>(std::floor(playerPos.x));
        int blockY = static_cast<int>(std::floor(playerPos.y - 0.1f));
        int blockZ = static_cast<int>(std::floor(playerPos.z));
        Block blockBelow = chunkManager.getBlockAt(blockX, blockY, blockZ);
        Audio::SoundType stepSound = Audio::getStepSoundForBlock(static_cast<uint8_t>(blockBelow.getType()));
        Audio::AudioManager::instance().playSound(stepSound, 0.6f);
    }

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos - lastX);
    float yoffset = static_cast<float>(lastY - ypos);

    lastX = xpos;
    lastY = ypos;

    if (std::abs(xoffset) < 0.1f) xoffset = 0.0f;
    if (std::abs(yoffset) < 0.1f) yoffset = 0.0f;

    camera.processMouseMovement(xoffset, yoffset);
}
