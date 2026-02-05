#define NOMINMAX
#include "RenderPipelineSystem.h"

#include "../Audio/AudioManager.h"
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
#include "../Render/Renderer.h"
#include "../Render/Camera.h"
#include "../Render/HeldItemRenderer.h"
#include "../Render/Shader.h"
#include "../World/ChunkManager.h"
#include "../World/WorldGenerator.h"
#include "../World/FireSystem.h"
#include "../Render/ExplosionVolumeSystem.h"
#include "../Render/BloodSplatterSystem.h"
#include "../Game/MenuWorldSystem.h"
#include "../UI/UIManager.h"
#include "../Physics/PhysicsTest.h"
#include "../Core/Logger.h"
#include <functional>

RenderPipelineSystem::RenderPipelineSystem(Renderer& rendererRef,
                                           ChunkManager& chunkManagerRef,
                                           MenuWorldSystem& menuWorldSystemRef,
                                           Camera& cameraRef,
                                           EntityManager& entityManagerRef,
                                           WorldGenerator& worldGeneratorRef,
                                           HeldItemRenderer& heldItemRendererRef,
                                           FireSystem& fireSystemRef,
                                           ExplosionVolumeSystem& explosionVolumesRef,
                                           BloodSplatterSystem& bloodSplatterRef,
                                           UIManager& uiManagerRef,
                                           Network::NetworkManager& networkManagerRef,
                                           Physics::PhysicsTestSystem& physicsTestRef,
                                           std::unique_ptr<PlayerEntity>& playerEntityRef,
                                           std::vector<std::unique_ptr<ZombieEntity>>& zombiesRef,
                                           std::vector<std::unique_ptr<SkeletonEntity>>& skeletonsRef,
                                           std::vector<std::unique_ptr<PigEntity>>& pigsRef,
                                           std::vector<std::unique_ptr<ChickenEntity>>& chickensRef,
                                           std::vector<std::unique_ptr<SheepEntity>>& sheepRef,
                                           bool& useNewEntityManagerRef)
        : renderer(rendererRef),
      chunkManager(chunkManagerRef),
      menuWorldSystem(menuWorldSystemRef),
      camera(cameraRef),
      entityManager(entityManagerRef),
      worldGenerator(worldGeneratorRef),
      heldItemRenderer(heldItemRendererRef),
      fireSystem(fireSystemRef),
      explosionVolumes(explosionVolumesRef),
      bloodSplatter(bloodSplatterRef),
      uiManager(uiManagerRef),
      networkManager(networkManagerRef),
      physicsTest(physicsTestRef),
      playerEntity(playerEntityRef),
      zombies(zombiesRef),
      skeletons(skeletonsRef),
      pigs(pigsRef),
      chickens(chickensRef),
      sheep(sheepRef),
      useNewEntityManager(useNewEntityManagerRef) {
}

void RenderPipelineSystem::setWindow(Window* windowRef) {
    window = windowRef;
}

void RenderPipelineSystem::renderFrame(bool isBreakingBlock, float blockBreakProgress, const glm::ivec3& breakingBlockPos) {
    if (!window) return;

    bool inMainMenu = !uiManager.isWorldLoaded() &&
                      (uiManager.getMenuState() == MenuState::MAIN_MENU ||
                       uiManager.getMenuState() == MenuState::MULTIPLAYER ||
                       uiManager.getMenuState() == MenuState::HOST_GAME ||
                       uiManager.getMenuState() == MenuState::JOIN_GAME ||
                       uiManager.getMenuState() == MenuState::NEW_GAME ||
                       uiManager.getMenuState() == MenuState::LOAD_GAME ||
                       uiManager.getMenuState() == MenuState::SETTINGS ||
                       uiManager.getMenuState() == MenuState::VIDEO_SETTINGS ||
                       uiManager.getMenuState() == MenuState::PLAYER_SETTINGS ||
                       uiManager.getMenuState() == MenuState::CONTROLS ||
                       uiManager.getMenuState() == MenuState::ABOUT);

    renderer.setShowCrosshair(!uiManager.isMenuOpen());

    if (inMainMenu) {
        renderMenuWorld();
        return;
    }

    std::vector<Entity*> entities;
    collectEntities(entities);

    auto remotePlayerEntities = networkManager.getRemotePlayerEntities();
    for (auto* remotePlayer : remotePlayerEntities) {
        entities.push_back(remotePlayer);
    }

    {
        glm::vec3 camPos = camera.getPosition();
        BiomeType currentBiome = worldGenerator.getBiome(camPos.x, camPos.z);
        BiomeInfo biomeInfo = worldGenerator.getBiomeInfo(currentBiome);

        renderer.setBiomeGrassColor(glm::vec3(biomeInfo.grassColorR, biomeInfo.grassColorG, biomeInfo.grassColorB));
        renderer.setBiomeFoliageColor(glm::vec3(biomeInfo.foliageColorR, biomeInfo.foliageColorG, biomeInfo.foliageColorB));
        renderer.setUseBiomeColors(true);
    }

    std::vector<Renderer::DebrisRenderData> debrisData;
    std::vector<Renderer::BloodDecalRenderData> bloodDecals;
    std::vector<Renderer::ModelBloodDecal> modelDecals;
#if ENABLE_PHYSICS_TEST
    if (physicsTest.isEnabled()) {
        debrisData = physicsTest.getDebrisRenderData();
        bloodDecals = physicsTest.getBloodDecalRenderData();
        modelDecals = physicsTest.getModelBloodDecals();
    }
    
    // Log limb debris count for debugging
    static int limbDebrisLogCount = 0;
    if (limbDebrisLogCount < 20 && physicsTest.getLimbDebrisCount() > 0) {
        LOG_INFO("[RenderPipeline] Limb debris count: " + std::to_string(physicsTest.getLimbDebrisCount()));
        limbDebrisLogCount++;
    }
#endif

    std::vector<glm::vec3> fireLights;
    fireSystem.getFireLightPositions(fireLights, 16);
    renderer.setFireLightPositions(fireLights);

    std::function<void(Shader&, const glm::vec3&, const glm::vec3&)> limbRenderPass;
#if ENABLE_PHYSICS_TEST
    if (physicsTest.isEnabled() && physicsTest.getLimbDebrisCount() > 0) {
        limbRenderPass = [this](Shader& modelShader, const glm::vec3& renderOrigin, const glm::vec3&) {
            modelShader.setInt("uUseShadows", 0);
            modelShader.setFloat("uAlphaMultiplier", 1.0f);
            physicsTest.renderLimbDebris(modelShader, renderOrigin);
        };
    }
#endif
    renderer.render(chunkManager, camera, entities, window->getWidth(), window->getHeight(), debrisData, bloodDecals, &explosionVolumes, limbRenderPass, modelDecals);

    renderer.cleanUnusedMeshes(chunkManager);
    renderer.blitDepthToScreen(window->getWidth(), window->getHeight());

    // Render blood splatter particles
    {
        float aspectRatio = static_cast<float>(window->getWidth()) / static_cast<float>(window->getHeight());
        glm::mat4 projection = glm::perspective(glm::radians(camera.getFov()), aspectRatio, 0.1f, 1000.0f);
        glm::mat4 view = camera.getViewMatrix();
        glm::vec3 cameraRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
        glm::vec3 cameraUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
        bloodSplatter.render(view, projection, cameraRight, cameraUp);
    }

    if (isBreakingBlock && !uiManager.isCreativeMode && blockBreakProgress > 0.0f) {
        renderer.renderBlockBreakOverlay(camera, breakingBlockPos, blockBreakProgress, window->getWidth(), window->getHeight());
    }

    renderHeldItems(remotePlayerEntities);

    if (!camera.isThirdPerson() && uiManager.isWorldLoaded() && !uiManager.isMenuOpen()) {
        Shader& modelShader = renderer.getModelShader();
        heldItemRenderer.renderFirstPerson(modelShader, camera, window->getWidth(), window->getHeight());
    }

    uiManager.render();
    uiManager.renderConsole();
}

void RenderPipelineSystem::renderMenuWorld() {
    if (!window) return;
    std::vector<Entity*> emptyEntities;
    renderer.setFireLightPositions({});
    renderer.render(chunkManager, menuWorldSystem.getCamera(), emptyEntities, window->getWidth(), window->getHeight(), {}, {}, nullptr);
    renderer.cleanUnusedMeshes(chunkManager);
    uiManager.render();
    uiManager.renderConsole();
}

void RenderPipelineSystem::collectEntities(std::vector<Entity*>& entities) {
    if (playerEntity) {
        glm::vec3 footPos = camera.getPosition();
        playerEntity->setPosition(footPos);
        playerEntity->setRotation(glm::vec3(0.0f, -camera.getYaw() + 90.0f, 0.0f));
        playerEntity->setVelocity(camera.velocity);

        if (camera.isThirdPerson()) {
            entities.push_back(playerEntity.get());
        }
    }

    if (useNewEntityManager) {
        auto managedEntities = entityManager.getAllEntities();
        entities.insert(entities.end(), managedEntities.begin(), managedEntities.end());
    } else {
        for (auto& z : zombies) {
            if (z) entities.push_back(z.get());
        }
        for (auto& s : skeletons) {
            if (s) entities.push_back(s.get());
        }
        for (auto& p : pigs) {
            if (p) entities.push_back(p.get());
        }
        for (auto& c : chickens) {
            if (c) entities.push_back(c.get());
        }
        for (auto& s : sheep) {
            if (s) entities.push_back(s.get());
        }
    }
}

void RenderPipelineSystem::renderHeldItems(const std::vector<Network::RemotePlayerEntity*>& remotePlayerEntities) {
    if (!uiManager.isWorldLoaded()) {
        return;
    }

    Shader& modelShader = renderer.getModelShader();

    for (auto* remotePlayer : remotePlayerEntities) {
        uint8_t heldItemId = remotePlayer->getHeldItem();
        if (heldItemId != 0) {
            ItemType itemType = static_cast<ItemType>(heldItemId);

            if (remotePlayer->supportsHoldAnimations()) {
                glm::mat4 handTransform = remotePlayer->getRightHandTransform();
                heldItemRenderer.renderThirdPersonWithBone(modelShader, camera, handTransform, itemType,
                                                           window->getWidth(), window->getHeight());
            } else {
                float playerYaw = remotePlayer->getRotation().y;
                heldItemRenderer.renderThirdPerson(modelShader, camera, remotePlayer->getPosition(),
                                                   playerYaw, itemType,
                                                   window->getWidth(), window->getHeight());
            }
        }
    }

    if (camera.isThirdPerson() && playerEntity) {
        ItemType currentHeldItem = uiManager.getSelectedItem();
        if (currentHeldItem != ItemType::NONE) {
            if (playerEntity->supportsHoldAnimations()) {
                glm::mat4 handTransform = playerEntity->getRightHandTransform();
                heldItemRenderer.renderThirdPersonWithBone(modelShader, camera, handTransform, currentHeldItem,
                                                           window->getWidth(), window->getHeight());
            } else {
                float playerYaw = playerEntity->getRotation().y;
                heldItemRenderer.renderThirdPerson(modelShader, camera, playerEntity->getPosition(),
                                                   playerYaw, currentHeldItem,
                                                   window->getWidth(), window->getHeight());
            }
        }
    }
}
