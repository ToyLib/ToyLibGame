#include "Shiro.h"

#include <cmath>

namespace {

toy::kit::HumanoidDesc MakeShiroDesc()
{
    toy::kit::HumanoidDesc desc;

    desc.model                     = "Enemy/Shiro.glb";
    desc.scale                     = 3.0f;
    desc.toonRender                = true;
    desc.contourColor              = Vector3(0.3f, 0.3f, 0.35f);
    desc.meshOffset                = Vector3(0.0f, 0.0f, 0.8f);
    desc.cancelRootTranslationBone = "Hip";

    desc.colliderOffset = Vector3(0.0f, 0.0f, 0.0f);
    desc.colliderScale  = Vector3(0.5f, 1.0f, 0.3f);
    desc.colliderFlags  = toy::C_GROUND | toy::C_WALL | toy::C_FOOT
                         | toy::C_HURTBOX | toy::C_ENEMY_TEAM;

    desc.displayName = "SHIRO";
    desc.fontPath    = "rounded-mplus-1c-bold.ttf";
    desc.nameYOffset = 4.0f;

    desc.candidateTexture = "candidate.png";
    desc.lockedTexture    = "lockon.png";

    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
Shiro::Shiro(toy::Application* app)
    : mBody(app, MakeShiroDesc())
{
    // Idle : 索敵して Chase へ
    // Chase: 追跡。見失ったら Idle へ（ヒステリシス x1.5）
    mFSM.Register(ShiroState::Idle,
        /* onUpdate */ [this](float)
        {
            if (HasTarget() && GetDistanceToTarget() < mDetectRange)
            {
                mFSM.To(ShiroState::Chase);
            }
        },
        /* onEnter */ [this] { mBody.PlayAnimation(ANIM_IDLE); }
    );

    mFSM.Register(ShiroState::Chase,
        /* onUpdate */ [this](float dt)
        {
            MoveTowardTarget(mMoveSpeed, dt);
            LookAtTarget();
            if (!HasTarget() || GetDistanceToTarget() > mDetectRange * 1.5f)
            {
                mFSM.To(ShiroState::Idle);
            }
        },
        /* onEnter */ [this] { mBody.PlayAnimation(ANIM_RUN); }
    );

    mFSM.Start(ShiroState::Idle);
}

//-----------------------------------------------------------------------------
void Shiro::Update(float deltaTime)
{
    mFSM.Update(deltaTime);
}

//-----------------------------------------------------------------------------
float Shiro::GetDistanceToTarget() const
{
    if (!mTarget) return 1.0e9f;

    const Vector3& self   = mBody.GetPosition();
    const Vector3& target = mTarget->GetPosition();
    const float dx = target.x - self.x;
    const float dz = target.z - self.z;
    return sqrtf(dx * dx + dz * dz);
}

//-----------------------------------------------------------------------------
void Shiro::MoveTowardTarget(float speed, float dt)
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
void Shiro::LookAtTarget()
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
