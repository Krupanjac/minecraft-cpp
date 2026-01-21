#pragma once

#include "PassiveMob.h"

class PigEntity : public PassiveMob {
public:
    PigEntity(const glm::vec3& startPos);
    ~PigEntity() override = default;

protected:
    void loadModel() override;
};
