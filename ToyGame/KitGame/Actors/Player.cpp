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

    desc.enableLockOnCombat = true; // プレイヤー操作: Field/Battle切換え・カメラ・索敵を有効化

    desc.sensorFovDeg           = 60.0f;
    desc.sensorMaxDist          = 40.0f;
    desc.sensorNearOverrideDist = 20.0f;

    desc.lockBreakDist    = 35.0f;
    desc.lockLostGraceSec = 0.7f;

    desc.footstepSound = "Hero/Walk.wav";

    desc.freezeCameraYInAir = true;

    return desc;
}

//=============================================================================
// PlayerControlBehavior
//  プレイヤーの入力（IBehavior::OnInput）を受けて、どのアニメーションを
//  再生するか（ゲーム固有の意味づけ）だけを担当する。
//  移動/カメラ切換え/ロックオンの選択・解除ロジックそのものは Humanoid が持つ
//  （Humanoid::OnUpdate 経由で自動的に処理される）ため、OnUpdate は不要。
//
//  Humanoid 固有のメソッド（SelectNextTarget 等）を使うため、Prefab& を
//  Humanoid& へ static_cast する。ChaseBehavior のような「どの Prefab でも
//  動く」汎用性は無いが、そもそも Player 用の振る舞いは Humanoid 専用でよい。
//=============================================================================
class PlayerControlBehavior : public toy::kit::IBehavior
{
public:
    void OnStart(toy::kit::Prefab& body) override
    {
        mBody = static_cast<toy::kit::Humanoid*>(&body);
    }

    void OnInput(toy::kit::Prefab& /*body*/, const toy::InputState& state) override
    {
        const int moveMotion = mBody->IsInBattle() ? H_WalkSS : H_Run;
        UpdateMovementAnimation(state, moveMotion);

        SelectTarget(state);

        if (state.IsButtonDown(toy::GameButton::L2))
        {
            InputAttack(state);
        }

        // Bでロック解除（L2と競合しないようガード）
        if (state.IsButtonPressed(toy::GameButton::B) && !state.IsButtonDown(toy::GameButton::L2))
        {
            mBody->ReleaseTarget();
        }
    }

private:
    // アニメーションID（Hero/hero_f.gltf に対応。旧 PlayerMotion）
    enum PlayerMotion
    {
        H_Dead     = 0,
        H_Guard    = 1,
        H_Hit1     = 2,
        H_Hit2     = 3,
        H_Jump     = 4,
        H_JumpSS   = 5,
        H_Kick     = 6,
        H_KickSS   = 7,
        H_Pump     = 9,
        H_PumpSS   = 10,
        H_Run      = 11,
        H_RunSS    = 12,
        H_Slash    = 13,
        H_Spin     = 14,
        H_Stab     = 15,
        H_Standing = 16,
        H_Stand    = 17,
        H_Walk     = 18,
        H_WalkSS   = 19
    };

    void SelectTarget(const toy::InputState& state)
    {
        if (state.IsButtonPressed(toy::GameButton::L1)) mBody->SelectPrevTarget();
        if (state.IsButtonPressed(toy::GameButton::R1)) mBody->SelectNextTarget();
    }

    void InputAttack(const toy::InputState& state)
    {
        if (!mBody->IsInBattle()) return;
        if (!mBody->IsMovable())  return;

        mBody->SetAnimPlayRate(1.5f);

        if (state.IsButtonPressed(toy::GameButton::B))
        {
            mBody->PlayAnimationOnce(H_Slash, H_Stand);
            mBody->SetMovable(false);
        }
        else if (state.IsButtonPressed(toy::GameButton::X))
        {
            mBody->PlayAnimationOnce(H_Spin, H_Stand);
            mBody->SetMovable(false);
        }
        else if (state.IsButtonPressed(toy::GameButton::Y))
        {
            mBody->PlayAnimationOnce(H_Stab, H_Stand);
            mBody->SetMovable(false);
        }
    }

    // UpdateMovementAnimation（旧 ApplyGroundMoveAndAnim 相当）
    //  移動入力自体は Humanoid 内の MoveComponent が自前で処理済み。
    //  ここでは現在の移動/接地状態からアニメーションだけを選ぶ。
    void UpdateMovementAnimation(const toy::InputState& state, int moveMotion)
    {
        mBody->SetAnimPlayRate(1.5f);

        if (!mBody->IsMovable())
        {
            // 攻撃中ロックからの復帰は Humanoid 側が自動で行う
            return;
        }

        if (state.IsButtonPressed(toy::GameButton::A))
        {
            mBody->Jump();
            mBody->PlayAnimationOnce(H_Jump, H_Stand);
        }

        if (mBody->GetVerticalVelocity() != 0.0f)
        {
            mBody->PlayAnimation(H_Jump);
        }
        else if (!mBody->IsMoving())
        {
            mBody->PlayAnimation(H_Stand);
        }
        else
        {
            mBody->PlayAnimation(moveMotion);
        }
    }

    toy::kit::Humanoid* mBody = nullptr;
};

} // namespace

//-----------------------------------------------------------------------------
std::unique_ptr<Player> MakePlayer(toy::Application* app)
{
    auto player = std::make_unique<Player>(app, std::make_unique<PlayerControlBehavior>(), MakeHeroDesc());
    player->GetBody().SetPosition(Vector3(0.0f, 30.0f, 0.0f));
    player->GetBody().SetRotation(Quaternion(Vector3::UnitY, Math::ToRadians(180.0f)));
    return player;
}
