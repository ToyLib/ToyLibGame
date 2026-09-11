#include "Noriko.h"

#include <cmath>
#include <cstdlib>

namespace {

toy::kit::CreatureDesc MakeNorikoDesc()
{
    toy::kit::CreatureDesc desc;
    desc.model                     = "Field/noriko.glb";
    desc.scale                     = 3.0f;
    desc.yawOffsetDeg              = 180.0f;
    desc.cancelRootTranslationBone = "Hip";

    desc.colliderOffset = Vector3(0.0f, 0.0f, 0.0f);
    desc.colliderScale  = Vector3(0.5f, 1.0f, 0.3f);
    desc.colliderFlags  = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                         | toy::C_HURTBOX | toy::C_ENEMY_TEAM;

    desc.displayName = "海苔子";

    desc.candidateTexture = "UI/candidate.png";
    desc.lockedTexture    = "UI/lockon.png";

    return desc;
}

//=============================================================================
// IdleWalkBehavior
//  Idle（静止）→Walk（ランダムな方向へ移動）→Idle をタイマーで切り替える
//  最小限の振る舞い。Walkに入るたびに新しいランダム方向を選び直す。
//  今のところ Noriko 専用（他の Creature でも使うようになったら
//  ToyKit 側の汎用 Behavior に昇格する。ChaseBehavior と同じ流れ）。
//=============================================================================
class IdleWalkBehavior : public toy::kit::IBehavior
{
public:
    void OnStart(toy::kit::Prefab& body) override
    {
        mBody = &body;

        mFSM.Register(State::Idle,
            [this](float)
            {
                if (mFSM.IsEnterFrame()) mBody->PlayAnimationBlend(0, kAnimBlendSec);
                if (mFSM.GetTimer() > 10.0f) mFSM.To(State::Walk);
            });

        mFSM.Register(State::Walk,
            [this](float dt)
            {
                if (mFSM.IsEnterFrame())
                {
                    mBody->PlayAnimationBlend(1, kAnimBlendSec);
                    PickRandomDirection();
                }
                MoveForward(dt);
                if (mFSM.GetTimer() > 5.0f) mFSM.To(State::Idle);
            });

        mFSM.Start(State::Idle);
    }

    void OnUpdate(toy::kit::Prefab& /*body*/, float deltaTime) override
    {
        mFSM.Update(deltaTime);
    }

private:
    enum class State { Idle, Walk };

    static constexpr float kAnimBlendSec = 0.5f;

    // XZ平面でランダムな方向を選び、その方向を向く
    void PickRandomDirection()
    {
        const float angle = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX))
                           * 2.0f * 3.14159265f;
        mMoveDir = Vector3(sinf(angle), 0.0f, cosf(angle));
        mBody->SetRotation(Quaternion(Vector3::UnitY, angle));
    }

    // Y は重力に任せ、XZ だけ現在の向きへ進める
    void MoveForward(float dt)
    {
        const Vector3& pos = mBody->GetPosition();
        mBody->SetPosition(pos + mMoveDir * mMoveSpeed * dt);
    }

    toy::kit::Prefab*                mBody    = nullptr;
    toy::kit::KitStateMachine<State> mFSM;
    Vector3                          mMoveDir  = Vector3::UnitZ;
    float                            mMoveSpeed = 3.0f;
};

} // namespace

//-----------------------------------------------------------------------------
std::unique_ptr<Noriko> MakeNoriko(toy::Application* app, const Vector3& position)
{
    auto noriko = std::make_unique<Noriko>(app, std::make_unique<IdleWalkBehavior>(), MakeNorikoDesc());
    noriko->GetBody().SetPosition(position);
    return noriko;
}
