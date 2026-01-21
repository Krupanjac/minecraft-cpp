#pragma once

#include "PassiveMob.h"

class SheepEntity : public PassiveMob {
public:
    SheepEntity(const glm::vec3& startPos);
    ~SheepEntity() override = default;

protected:
    void loadModel() override;
};
