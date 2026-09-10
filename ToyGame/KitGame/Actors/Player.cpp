#include "Player.h"

namespace {

toy::kit::HumanoidDesc MakeHeroDesc()
{
    toy::kit::HumanoidDesc desc;

    desc.model         = "Hero/hero_f.gltf";
    desc.scale          = 0.1f;
    desc.yawOffsetDeg    = 180.0f;
    desc.toonRender      = true;
    desc.contourFactor   = 1.01f;
    desc.contourColor    = Vector3(0.2f, 0.2f, 0.2f);

    desc.colliderOffset = Vector3(0.0f, 0.0f, 0.0f);
    desc.colliderScale  = Vector3(0.5f, 1.0f, 0.4f);
    desc.colliderFlags  = toy::C_FOOT | toy::C_BODY | toy::C_PLAYER_TEAM;

    desc.enableGroundPose = false; // アニメは自前で制御

    desc.sensorFovDeg           = 60.0f;
    desc.sensorMaxDist          = 40.0f;
    desc.sensorNearOverrideDist = 20.0f;

    desc.lockBreakDist    = 35.0f;
    desc.lockLostGraceSec = 0.7f;

    desc.footstepSound = "Hero/Walk.wav";

    desc.freezeCameraYInAir = true;

    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
Player::Player(toy::Application* app)
    : mBody(app, MakeHeroDesc())
{
    mBody.SetPosition(Vector3(0.0f, 30.0f, 0.0f));
    mBody.SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(180.0f)));
}

//-----------------------------------------------------------------------------
void Player::ProcessInput(const toy::InputState& state)
{
    const int moveMotion = mBody.IsInBattle() ? H_WalkSS : H_Run;
    UpdateMovementAnimation(state, moveMotion);

    SelectTarget(state);

    if (state.IsButtonDown(toy::GameButton::L2))
    {
        InputAttack(state);
    }

    // Bでロック解除（L2と競合しないようガード）
    if (state.IsButtonPressed(toy::GameButton::B) && !state.IsButtonDown(toy::GameButton::L2))
    {
        mBody.ReleaseTarget();
    }
}

//-----------------------------------------------------------------------------
void Player::Update(float /*deltaTime*/)
{
    // 移動・カメラ切換え・ロックオン判定そのものは Humanoid::OnUpdate が行う。
}

//-----------------------------------------------------------------------------
void Player::SelectTarget(const toy::InputState& state)
{
    if (state.IsButtonPressed(toy::GameButton::L1)) mBody.SelectPrevTarget();
    if (state.IsButtonPressed(toy::GameButton::R1)) mBody.SelectNextTarget();
}

//-----------------------------------------------------------------------------
void Player::InputAttack(const toy::InputState& state)
{
    if (!mBody.IsInBattle()) return;
    if (!mBody.IsMovable())  return;

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
    }
    else if (state.IsButtonPressed(toy::GameButton::Y))
    {
        mBody.PlayAnimationOnce(H_Stab, H_Stand);
        mBody.SetMovable(false);
    }
}

//-----------------------------------------------------------------------------
// UpdateMovementAnimation（旧 ApplyGroundMoveAndAnim 相当）
//  移動入力自体は Humanoid 内の MoveComponent が自前で処理済み。
//  ここでは現在の移動/接地状態からアニメーションだけを選ぶ。
//-----------------------------------------------------------------------------
void Player::UpdateMovementAnimation(const toy::InputState& state, int moveMotion)
{
    mBody.SetAnimPlayRate(1.5f);

    if (!mBody.IsMovable())
    {
        // 攻撃中ロックからの復帰は Humanoid 側が自動で行う
        return;
    }

    if (state.IsButtonPressed(toy::GameButton::A))
    {
        mBody.Jump();
        mBody.PlayAnimationOnce(H_Jump, H_Stand);
    }

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
        mBody.PlayAnimation(moveMotion);
    }
}
