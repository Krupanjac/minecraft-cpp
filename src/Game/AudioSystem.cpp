#define NOMINMAX
#include "AudioSystem.h"

#include "../Audio/AudioManager.h"
#include "../Core/Logger.h"
#include "../Core/Settings.h"
#include "../Render/Camera.h"
#include "../UI/UIManager.h"
#include "../World/Block.h"
#include "../World/ChunkManager.h"

#include <cmath>

AudioSystem::AudioSystem(UIManager& uiManagerRef,
                         Camera& cameraRef,
                         ChunkManager& chunkManagerRef,
                         bool& isUnderwaterRef,
                         bool& wasUnderwaterRef)
    : uiManager(uiManagerRef),
      camera(cameraRef),
      chunkManager(chunkManagerRef),
      isUnderwater(isUnderwaterRef),
      wasUnderwater(wasUnderwaterRef) {
}

bool AudioSystem::initialize() {
    if (!Audio::AudioManager::instance().initialize()) {
        LOG_WARNING("Failed to initialize audio system - continuing without sound");
        return false;
    }

    Audio::AudioManager::instance().loadAllSounds();

    auto& settings = Settings::instance();
    Audio::AudioManager::instance().setMasterVolume(settings.masterVolume);
    Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MUSIC, settings.musicVolume);
    Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::AMBIENT, settings.ambientVolume);
    // soundVolume controls BLOCKS, MOBS, PLAYER, and UI categories
    Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::BLOCKS, settings.soundVolume);
    Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::MOBS, settings.soundVolume);
    Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::PLAYER, settings.soundVolume);
    Audio::AudioManager::instance().setCategoryVolume(Audio::SoundCategory::UI, settings.soundVolume);

    return true;
}

void AudioSystem::update(float deltaTime) {
    updateAmbientState();
    updateFootsteps(deltaTime);
    updateSwimSounds(deltaTime);

    Audio::AudioManager::instance().setListenerPosition(
        camera.getPosition(),
        camera.getFront(),
        camera.getUp()
    );
    Audio::AudioManager::instance().update(deltaTime);
}

void AudioSystem::updateAmbientState() {
    if (uiManager.isWorldLoaded()) {
        glm::vec3 headPos = camera.getPosition() + glm::vec3(0.0f, camera.defaultY, 0.0f);
        int hx = static_cast<int>(std::floor(headPos.x));
        int hy = static_cast<int>(std::floor(headPos.y));
        int hz = static_cast<int>(std::floor(headPos.z));

        Block headBlock = chunkManager.getBlockAt(hx, hy, hz);
        bool underwater = headBlock.getType() == BlockType::WATER;
        wasUnderwater = isUnderwater;
        isUnderwater = underwater;

        Block ceilingBlock = chunkManager.getBlockAt(hx, hy + 2, hz);
        bool inCave = ceilingBlock.getType() != BlockType::AIR && ceilingBlock.getType() != BlockType::WATER;

        Audio::AudioManager::instance().setUnderwater(underwater);
        Audio::AudioManager::instance().setInCave(inCave);
    } else {
        wasUnderwater = isUnderwater;
        isUnderwater = false;
        Audio::AudioManager::instance().setUnderwater(false);
        Audio::AudioManager::instance().setInCave(false);
    }
}

void AudioSystem::updateFootsteps(float deltaTime) {
    if (camera.onGround && !camera.isFlying && uiManager.isWorldLoaded() && !isUnderwater) {
        float horizontalSpeed = glm::length(glm::vec2(camera.velocity.x, camera.velocity.z));
        if (horizontalSpeed > 0.5f) {
            footstepTimer -= deltaTime;
            if (footstepTimer <= 0.0f) {
                glm::vec3 playerPos = camera.getPosition();
                int blockX = static_cast<int>(std::floor(playerPos.x));
                int blockY = static_cast<int>(std::floor(playerPos.y - 0.1f));
                int blockZ = static_cast<int>(std::floor(playerPos.z));
                Block blockBelow = chunkManager.getBlockAt(blockX, blockY, blockZ);

                Audio::SoundType stepSound = Audio::getStepSoundForBlock(static_cast<uint8_t>(blockBelow.getType()));
                Audio::AudioManager::instance().playSound(stepSound, 0.5f);

                footstepInterval = camera.isSprinting ? 0.3f : 0.45f;
                footstepTimer = footstepInterval;
            }
        } else {
            footstepTimer = 0.0f;
        }
    }
}

void AudioSystem::updateSwimSounds(float deltaTime) {
    if (uiManager.isWorldLoaded()) {
        if (isUnderwater && !wasUnderwater) {
            Audio::AudioManager::instance().playSound(Audio::SoundType::WATER_SPLASH, 0.6f);
            swimTimer = 0.0f;
        }

        float swimSpeed = glm::length(glm::vec2(camera.velocity.x, camera.velocity.z));
        if (isUnderwater && swimSpeed > 0.2f) {
            swimTimer -= deltaTime;
            if (swimTimer <= 0.0f) {
                Audio::AudioManager::instance().playSound(Audio::SoundType::WATER_SWIM, 0.5f);
                swimInterval = camera.isSprinting ? 0.45f : 0.7f;
                swimTimer = swimInterval;
            }
        } else if (!isUnderwater) {
            swimTimer = 0.0f;
        }
    }
}
