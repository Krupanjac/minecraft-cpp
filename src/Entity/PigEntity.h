#pragma once

#include "PassiveMob.h"
#include <memory>

namespace ModelSystem { class Model; }

class PigEntity : public PassiveMob {
public:
    // Original constructor (loads model internally)
    PigEntity(const glm::vec3& startPos);
    
    // New constructor with pre-loaded model (no stutter)
    PigEntity(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id = 0);
    
    ~PigEntity() override = default;

protected:
    void loadModel() override;
    
    // Sound overrides
    Audio::SoundType getAmbientSound() const override { return Audio::SoundType::MOB_PIG_SAY; }
    Audio::SoundType getHurtSound() const override { return Audio::SoundType::MOB_PIG_SAY; }
    Audio::SoundType getDeathSound() const override { return Audio::SoundType::MOB_PIG_DEATH; }
    
private:
    void initializeCommon();
};
