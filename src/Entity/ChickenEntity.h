#pragma once

#include "PassiveMob.h"
#include <memory>

namespace ModelSystem { class Model; }

class ChickenEntity : public PassiveMob {
public:
    // Original constructor (loads model internally)
    ChickenEntity(const glm::vec3& startPos);
    
    // New constructor with pre-loaded model (no stutter)
    ChickenEntity(const glm::vec3& startPos, std::shared_ptr<ModelSystem::Model> cachedModel, EntityId id = 0);
    
    ~ChickenEntity() override = default;

protected:
    void loadModel() override;
    
private:
    void initializeCommon();
};
