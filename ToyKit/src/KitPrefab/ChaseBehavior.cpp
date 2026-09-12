#include "KitPrefab/ChaseBehavior.h"
#include "KitPrefab/Prefab.h"
#include "KitPrefab/MovementUtil.h"

namespace toy::kit {

//-----------------------------------------------------------------------------
void ChaseBehavior::OnStart(Prefab& body)
{
    mBody = &body;

    // Idle : 索敵して Chase へ
    // Chase: 追跡。見失ったら Idle へ（ヒステリシス）
    mFSM.Register(State::Idle,
        /* onUpdate */ [this](float)
        {
            if (HasTarget() && GetDistanceXZ(*mBody, mTarget->GetPosition()) < mDesc.detectRange)
            {
                mFSM.To(State::Chase);
            }
        },
        /* onEnter */ [this] { mBody->PlayAnimationBlend(mDesc.idleAnim, mDesc.animBlendSec); }
    );

    mFSM.Register(State::Chase,
        /* onUpdate */ [this](float dt)
        {
            if (HasTarget())
            {
                MoveTowardPointXZ(*mBody, mTarget->GetPosition(), mDesc.moveSpeed, dt, mDesc.stopRange);
                FaceTowardPointXZ(*mBody, mTarget->GetPosition());
            }
            if (!HasTarget() || GetDistanceXZ(*mBody, mTarget->GetPosition()) > mDesc.detectRange * mDesc.loseRangeMultiplier)
            {
                mFSM.To(State::Idle);
            }
        },
        /* onEnter */ [this] { mBody->PlayAnimationBlend(mDesc.chaseAnim, mDesc.animBlendSec); }
    );

    mFSM.Start(State::Idle);
}

//-----------------------------------------------------------------------------
void ChaseBehavior::OnUpdate(Prefab& /*body*/, float deltaTime)
{
    mFSM.Update(deltaTime);
}

} // namespace toy::kit
