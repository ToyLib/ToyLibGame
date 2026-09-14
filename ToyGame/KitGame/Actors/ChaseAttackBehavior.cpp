#include "ChaseAttackBehavior.h"

using namespace toy::kit;

//-----------------------------------------------------------------------------
void ChaseAttackBehavior::OnStart(Prefab& body)
{
    mBody = static_cast<Humanoid*>(&body);

    // Idle: 視野センサーに捉えたら Chase へ。
    mFSM.Register(State::Idle,
        /* onUpdate */ [this](float)
        {
            if (HasTarget() && mBody->HasSensorHit())
            {
                mFSM.To(State::Chase);
            }
        },
        /* onEnter */ [this] { mBody->PlayAnimationBlend(mDesc.idleAnim, mDesc.animBlendSec); }
    );

    // Chase: 追従しつつ接近（＝Lockon的な振る舞いを兼ねる。センサーは再チェックしない）。
    //   attackRange まで詰めたら Attack へ。見失ったら（離れすぎ／ターゲット消失・撃破）Idle へ。
    //   間合い内（クールダウン待ち含む）はIdleアニメ、間合い外は追跡アニメに切り替える
    //   （切り替わった時だけ PlayAnimationBlend を呼ぶ。毎フレーム呼ぶとブレンドが途切れるため）。
    mFSM.Register(State::Chase,
        /* onUpdate */ [this](float dt)
        {
            if (!HasTarget())
            {
                mFSM.To(State::Idle);
                return;
            }

            MoveTowardPointXZ(*mBody, mTarget->GetPosition(), mDesc.moveSpeed, dt, mDesc.attackRange);
            FaceTowardPointXZ(*mBody, mTarget->GetPosition());

            const float dist    = GetDistanceXZ(*mBody, mTarget->GetPosition());
            const bool  inRange = (dist <= mDesc.attackRange);

            if (inRange != mWasInAttackRange || mFSM.IsEnterFrame())
            {
                mBody->PlayAnimationBlend(inRange ? mDesc.idleAnim : mDesc.chaseAnim, mDesc.animBlendSec);
            }
            mWasInAttackRange = inRange;

            if (inRange && mFSM.GetTimer() >= mDesc.attackCooldownSec)
            {
                mFSM.To(State::Attack);
            }
            else if (dist > mDesc.loseDistance * mDesc.loseRangeMultiplier)
            {
                mFSM.To(State::Idle);
            }
        }
    );

    // Attack: 攻撃モーション＋攻撃コライダー有効化。movable復帰（＝アニメ終了、
    //   Humanoid::UpdateMovableRecovery が自動でやってくれる）で Chase に戻る。
    mFSM.Register(State::Attack,
        /* onUpdate */ [this](float)
        {
            if (mBody->IsMovable())
            {
                mBody->SetAttackColliderActive(false);
                mFSM.To(State::Chase);
            }
        },
        /* onEnter */ [this]
        {
            mBody->SetMovable(false);
            mBody->PlayAnimationOnce(mDesc.attackAnim, mDesc.idleAnim);
            mBody->SetAttackColliderActive(true);
        }
    );

    mFSM.Start(State::Idle);
}

//-----------------------------------------------------------------------------
void ChaseAttackBehavior::OnUpdate(Prefab& /*body*/, float deltaTime)
{
    mFSM.Update(deltaTime);
}
