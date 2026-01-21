#pragma once

#include "PassiveMob.h"

class ChickenEntity : public PassiveMob {
public:
    ChickenEntity(const glm::vec3& startPos);
    ~ChickenEntity() override = default;

protected:
    void loadModel() override;
};
