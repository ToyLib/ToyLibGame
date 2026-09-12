#include "KitPrefab/ChaseBehavior.h"
#include "KitPrefab/Prefab.h"
#include "KitPrefab/MovementUtil.h"

namespace toy::kit {

//-----------------------------------------------------------------------------
void ChaseBehavior::OnStart(Prefab& body)
{
    mBody = &body;

    // Idle : body の視界センサー（HasSensorHit）に捉えたら Chase へ。
    //        ただし stopRange 以内はまだ Chase へ戻さない——Chase 側で
    //        「近づいたので Idle に戻った」直後は、向きを変えていなければ
    //        まだ視界に入ったままなので、これが無いと Idle⇔Chase を毎フレーム
    //        往復してしまう。
    // Chase: 追跡。見失ったら（detectRange*loseRangeMultiplierより離れたら）
    //        か、近づきすぎたら（stopRange以内）Idle へ戻る。
    //        近づいた後どうするか（攻撃 等）は今のところ決めていないため、
    //        とりあえず Idle に戻すだけにしている。
    mFSM.Register(State::Idle,
        /* onUpdate */ [this](float)
        {
            if (HasTarget()
                && mBody->HasSensorHit()
                && GetDistanceXZ(*mBody, mTarget->GetPosition()) > mDesc.stopRange)
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

                const float dist = GetDistanceXZ(*mBody, mTarget->GetPosition());
                if (dist <= mDesc.stopRange || dist > mDesc.detectRange * mDesc.loseRangeMultiplier)
                {
                    mFSM.To(State::Idle);
                }
            }
            else
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
