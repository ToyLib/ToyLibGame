#include "Physics/SensorComponent.h"
#include "Physics/PhysWorld.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"
#include "Physics/ColliderComponent.h"

namespace toy {

SensorComponent::SensorComponent(Actor* owner, int order)
    : Component(owner, order)
{
}

SensorComponent::SensorComponent(Actor* owner, const Desc& d, int order)
    : Component(owner, order)
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


    mPrev = mCurr;
    mCurr.clear();
    
    phys->QueryView(q, mHits);
    
    
    for (auto& h : mHits)
    {
        mCurr.push_back(h.collider);
    }

    // Enter
    for (auto* c : mCurr)
    {
        if (std::find(mPrev.begin(), mPrev.end(), c) == mPrev.end())
        {
            c->SetTargetState(TargetState::Candidate);
        }
    }

    // Leave
    for (auto* c : mPrev)
    {
        if (std::find(mCurr.begin(), mCurr.end(), c) == mCurr.end())
        {
            c->SetTargetState(TargetState::None);
        }
    }
}

} // namespace toy
