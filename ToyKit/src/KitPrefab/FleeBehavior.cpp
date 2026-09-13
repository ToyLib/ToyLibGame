#include "KitPrefab/FleeBehavior.h"
#include "KitPrefab/Prefab.h"
#include "KitPrefab/MovementUtil.h"

namespace toy::kit {

//-----------------------------------------------------------------------------
void FleeBehavior::OnStart(Prefab& body)
{
    mBody = &body;

    // Idle : body の視界センサー（HasSensorHit）に捉えたら Flee へ。
    //        入った瞬間、ターゲットの方を向く（見失って落ち着く動作）。
    // Flee : ターゲットと反対方向を向いて進む。loseDistance より離れたら
    //        Idle へ戻る（Flee 中は背を向けているので Sensor では判定しない。
    //        クラスコメント参照）。
    mFSM.Register(State::Idle,
        /* onUpdate */ [this](float)
        {
            if (HasTarget() && mBody->HasSensorHit())
            {
                mFSM.To(State::Flee);
            }
        },
        /* onEnter */ [this]
        {
            mBody->PlayAnimationBlend(mDesc.idleAnim, mDesc.animBlendSec);

            if (HasTarget())
            {
                const Vector3 dir = DirectionTowardPointXZ(*mBody, mTarget->GetPosition());
                FaceDirectionXZ(*mBody, dir);
            }
        }
    );

    mFSM.Register(State::Flee,
        /* onUpdate */ [this](float dt)
        {
            if (HasTarget())
            {
                const Vector3 dir = DirectionAwayFromPointXZ(*mBody, mTarget->GetPosition());
                FaceDirectionXZ(*mBody, dir);
                MoveInDirectionXZ(*mBody, dir, mDesc.moveSpeed, dt);
            }

            if (!HasTarget() || GetDistanceXZ(*mBody, mTarget->GetPosition()) > mDesc.loseDistance)
            {
                mFSM.To(State::Idle);
            }
        },
        /* onEnter */ [this] { mBody->PlayAnimationBlend(mDesc.fleeAnim, mDesc.animBlendSec); }
    );

    mFSM.Start(State::Idle);
}

//-----------------------------------------------------------------------------
void FleeBehavior::OnUpdate(Prefab& /*body*/, float deltaTime)
{
    mFSM.Update(deltaTime);
}

} // namespace toy::kit
