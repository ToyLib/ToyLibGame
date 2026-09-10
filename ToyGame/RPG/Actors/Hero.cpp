#include "Hero.h"
#include "MagicActor.h"
#include "HealMagicActor.h"

namespace {

toy::kit::HumanoidDesc MakeHeroDesc()
{
    toy::kit::HumanoidDesc desc;

    desc.model         = "Hero/hero_m.fbx";
    desc.scale          = 0.001f;
    desc.toonRender      = true;
    desc.contourFactor   = 1.01f;
    desc.contourColor    = Vector3(0.2f, 0.2f, 0.2f);

    desc.colliderOffset = Vector3(0.0f, 0.0f, 0.0f);
    desc.colliderScale  = Vector3(0.5f, 1.0f, 0.4f);
    desc.colliderFlags  = toy::C_FOOT | toy::C_BODY | toy::C_GROUND | toy::C_WALL | toy::C_PLAYER_TEAM;

    desc.enableGroundPose = false;
    desc.gravityAccel     = -80.0f;
    desc.jumpSpeed        = 35.0f;

    desc.enableLockOnCombat = true; // L1/R1で切替、L2+ボタンで攻撃

    desc.sensorFovDeg           = 180.0f;
    desc.sensorMaxDist          = 90.0f;
    desc.sensorNearOverrideDist = 30.0f;

    desc.footstepSound  = "Walk.wav";
    desc.footstepVolume = 0.5f;

    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
Hero::Hero(toy::Application* app)
    : mBody(app, MakeHeroDesc())
{
    mBody.SetPosition(Vector3(0.0f, 30.0f, 0.0f));
    mBody.SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(180.0f)));

    // 魔法アクター（未移行。当面 Application に直接生成する元の挙動を維持）
    mMagic = app->CreateActor<MagicActor>();
    mHeal  = app->CreateActor<HealMagicActor>();
}

//-----------------------------------------------------------------------------
void Hero::ProcessInput(const toy::InputState& state)
{
    SelectTarget(state);

    if (state.IsButtonDown(toy::GameButton::L2))
    {
        OnAttackInput(state);
    }

    if (state.IsButtonPressed(toy::GameButton::B) && !state.IsButtonDown(toy::GameButton::L2))
    {
        mBody.ReleaseTarget();
    }

    OnPlayerInput(state);
}

//-----------------------------------------------------------------------------
void Hero::Update(float deltaTime)
{
    UpdatePlayerAnim(deltaTime);
}

//-----------------------------------------------------------------------------
void Hero::SelectTarget(const toy::InputState& state)
{
    if (state.IsButtonPressed(toy::GameButton::L1)) mBody.SelectPrevTarget();
    if (state.IsButtonPressed(toy::GameButton::R1)) mBody.SelectNextTarget();
}

//-----------------------------------------------------------------------------
// OnAttackInput（L2 押下中に呼ばれる。Field/Battleどちらでも攻撃可 = 元仕様）
//-----------------------------------------------------------------------------
void Hero::OnAttackInput(const toy::InputState& state)
{
    if (!mBody.IsMovable()) return;

    mBody.SetAnimPlayRate(1.5f);

    if (state.IsButtonPressed(toy::GameButton::B))
    {
        mBody.PlayAnimationOnce(H_Slash, H_Stand);
        mBody.SetMovable(false);
    }
    else if (state.IsButtonPressed(toy::GameButton::X))
    {
        mBody.PlayAnimationOnce(H_Spin, H_Stand);
        mBody.SetMovable(false);
        mHeal->Spawn(mBody.GetPosition());
    }
    else if (state.IsButtonPressed(toy::GameButton::Y))
    {
        mBody.PlayAnimationOnce(H_Stab, H_Stand);
        mBody.SetMovable(false);
        mMagic->Spawn(mBody.GetPosition(), mBody.GetForward());
    }
}

//-----------------------------------------------------------------------------
void Hero::OnPlayerInput(const toy::InputState& state)
{
    if (!mBody.IsMovable()) return;

    if (state.IsButtonPressed(toy::GameButton::A))
    {
        mBody.Jump();
        mBody.PlayAnimationOnce(H_Jump, H_Stand);
    }
}

//-----------------------------------------------------------------------------
// UpdatePlayerAnim（毎フレーム。攻撃中ロックからの復帰は Humanoid 側が自動で行う）
//-----------------------------------------------------------------------------
void Hero::UpdatePlayerAnim(float /*deltaTime*/)
{
    mBody.SetAnimPlayRate(1.5f);

    if (!mBody.IsMovable()) return;

    if (mBody.GetVerticalVelocity() != 0.0f)
    {
        mBody.PlayAnimation(H_Jump);
    }
    else if (!mBody.IsMoving())
    {
        mBody.PlayAnimation(H_Stand);
    }
    else
    {
        // バトルモード（ロックオン中）はストレイフ用アニメ
        const int moveMotion = mBody.IsInBattle() ? H_WalkSS : H_Run;
        mBody.PlayAnimation(moveMotion);
    }
}
