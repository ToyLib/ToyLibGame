#pragma once

#include "Engine/Core/Component.h"
#include "Physics/ColliderFlags.h"
#include "Utils/MathUtil.h"

#include <vector>

namespace toy {

class SensorComponent : public Component
{
public:
    struct Desc
    {
        float    fovRad      = Math::ToRadians(90.0f);
        float    maxDist     = 10.0f;
        uint32_t targetMask  = C_ENEMY_TEAM | C_HURTBOX;
        uint32_t losBlock    = C_WALL;
        bool     requireLOS  = true;
    };
    SensorComponent(Actor* owner, int order = 5);
    SensorComponent(class Actor* owner, const Desc& d, int order = 5);

    void Update(float dt) override;

    const std::vector<struct ViewQueryHit>& GetHits() const { return mHits; }

    Actor* GetNearestActor() const;
    Actor* GetBestTarget() const;

private:
    Desc mDesc{};
    std::vector<struct ViewQueryHit> mHits;
};

} // namespace toy
