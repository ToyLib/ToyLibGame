#include "Physics/SensorComponent.h"
#include "Physics/PhysWorld.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"

namespace toy {

SensorComponent::SensorComponent(Actor* owner, int order)
    : Component(owner, order)
    , mDesc{}
{
}

SensorComponent::SensorComponent(Actor* a, const Desc& d, int order)
    : Component(a, order)
    , mDesc(d)
{
}

void SensorComponent::Update(float)
{
    auto* phys = GetOwner()->GetApp()->GetPhysWorld();
    if (!phys)
    {
        return;
    }
    ViewQueryDesc q;
    q.origin       = GetOwner()->GetPosition();
    q.forward      = GetOwner()->GetForward();
    q.fovRad       = mDesc.fovRad;
    q.maxDist      = mDesc.maxDist;
    q.flagMask     = mDesc.targetMask;
    q.losBlockMask = mDesc.losBlock;
    q.requireLOS   = mDesc.requireLOS;
    q.ignoreActor  = GetOwner();
    q.nearOverrideDist = mDesc.nearOverrideDist;
    q.nearOverrideRequireLOS = mDesc.nearOverrideRequireLOS;

    phys->QueryView(q, mHits);
}

} // namespace toy
