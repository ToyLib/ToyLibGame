#include "Wolf.h"

#include <cmath>

namespace {

toy::kit::HumanoidDesc MakeWolfDesc()
{
    toy::kit::HumanoidDesc desc;

    desc.model         = "Enemy/wolf.gltf";
    desc.meshDrawOrder = 1000;

    desc.colliderFlags = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                        | toy::C_HURTBOX | toy::C_ENEMY_TEAM;

    desc.candidateTexture = "target_scope.png"; // ロック中は専用スプライト無し（元仕様）

    desc.ambientSound       = "growling.wav";
    desc.ambientSoundVolume = 0.2f;

    desc.speechText     = "Bow \nwow !";
    desc.speechFontPath = "rounded-mplus-1c-bold.ttf";
    desc.speechColor    = Vector3(1.0f, 0.0f, 0.0f);

    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
Wolf::Wolf(toy::Application* app)
    : mBody(app, MakeWolfDesc())
{
    mBody.SetScale(3.0f);

    // Idle : 索敵して Chase へ
    // Chase: 追跡。見失ったら Idle へ（ヒステリシス x1.5）
    mFSM.Register(WolfState::Idle,
        /* onUpdate */ [this](float)
        {
            if (HasTarget() && GetDistanceToTarget() < mDetectRange)
            {
                mFSM.To(WolfState::Chase);
            }
        },
        /* onEnter */ [this] { mBody.PlayAnimation(ANIM_IDLE); }
    );

    mFSM.Register(WolfState::Chase,
        /* onUpdate */ [this](float dt)
        {
            MoveTowardTarget(mMoveSpeed, dt);
            LookAtTarget();
            if (!HasTarget() || GetDistanceToTarget() > mDetectRange * 1.5f)
            {
                mFSM.To(WolfState::Idle);
            }
        },
        /* onEnter */ [this] { mBody.PlayAnimation(ANIM_RUN); }
    );

    mFSM.Start(WolfState::Idle);
}

//-----------------------------------------------------------------------------
void Wolf::Update(float deltaTime)
{
    mFSM.Update(deltaTime);
}

//-----------------------------------------------------------------------------
float Wolf::GetDistanceToTarget() const
{
    if (!mTarget) return 1.0e9f;

    const Vector3& self   = mBody.GetPosition();
    const Vector3& target = mTarget->GetPosition();
    const float dx = target.x - self.x;
    const float dz = target.z - self.z;
    return sqrtf(dx * dx + dz * dz);
}

//-----------------------------------------------------------------------------
void Wolf::MoveTowardTarget(float speed, float dt)
{
    if (!mTarget) return;

    const Vector3& self   = mBody.GetPosition();
    const Vector3& target = mTarget->GetPosition();
    const float dx   = target.x - self.x;
    const float dz   = target.z - self.z;
    const float dist = sqrtf(dx * dx + dz * dz);
    if (dist < mStopRange) return;

    const float step = speed * dt / dist;
    mBody.SetPosition(Vector3(self.x + dx * step,
                              self.y,               // Y は重力に任せる
                              self.z + dz * step));
}

//-----------------------------------------------------------------------------
void Wolf::LookAtTarget()
{
    if (!mTarget) return;

    const Vector3& self   = mBody.GetPosition();
    const Vector3& target = mTarget->GetPosition();
    const float dx = target.x - self.x;
    const float dz = target.z - self.z;
    if (dx * dx + dz * dz < 0.01f) return;

    const float angle = atan2f(-dx, -dz);
    mBody.SetRotation(Quaternion(Vector3::UnitY, angle));
}
