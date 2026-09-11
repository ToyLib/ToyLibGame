#include "Noriko.h"

namespace {

toy::kit::CreatureDesc MakeNorikoDesc()
{
    toy::kit::CreatureDesc desc;
    desc.model                     = "Field/noriko.glb";
    desc.scale                     = 3.0f;
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

} // namespace

//-----------------------------------------------------------------------------
Noriko::Noriko(toy::Application* app, const Vector3& position)
    : mBody(app, MakeNorikoDesc())
{
    mBody.SetPosition(position);

    mFSM.Register(MonsterState::Idle,
        [this](float /*dt*/)
        {
            if (mFSM.IsEnterFrame()) mBody.PlayAnimation(0);
            if (mFSM.GetTimer() > 10.0f) mFSM.To(MonsterState::Walk);
        });

    mFSM.Register(MonsterState::Walk,
        [this](float /*dt*/)
        {
            if (mFSM.IsEnterFrame()) mBody.PlayAnimation(1);
            if (mFSM.GetTimer() > 5.0f) mFSM.To(MonsterState::Idle);
        });

    mFSM.Start(MonsterState::Idle);
}

//-----------------------------------------------------------------------------
void Noriko::Update(float deltaTime)
{
    mFSM.Update(deltaTime);
}
