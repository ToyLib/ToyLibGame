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
        float    maxDist     = 50.0f;
        uint32_t targetMask  = C_ENEMY_TEAM | C_HURTBOX;
        uint32_t losBlock    = C_WALL | C_GROUND;
        bool     requireLOS  = false;
        
        float    nearOverrideDist = 8.0f;
        bool     nearOverrideRequireLOS = false;
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
    
    std::vector<class ColliderComponent*> mPrev;
    std::vector<class ColliderComponent*> mCurr;
};

} // namespace toy
