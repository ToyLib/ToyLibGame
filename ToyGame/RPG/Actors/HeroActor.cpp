#include "HeroActor.h"
#include "MagicActor.h"
#include "HealMagicActor.h"
#include "ToyLib.h"

#include <fstream>

HeroActor::HeroActor(toy::Application* a)
    : toy::kit::KitPlayerActor(a)
{
    //------------------------------------------------------------------
    // JSON 設定読み込み
    //------------------------------------------------------------------
    std::ifstream file("ToyGame/Settings/HeroActor.json");
    nlohmann::json json;
    file >> json;

    //------------------------------------------------------------------
    // メッシュ
    //------------------------------------------------------------------
    std::string meshPath;
    JsonHelper::GetString(json["mesh"], "file", meshPath);
    SetupMesh(meshPath);

    bool  useToon = false;
    float contour = 1.0f;
    JsonHelper::GetBool (json["mesh"], "toon_render",    useToon);
    JsonHelper::GetFloat(json["mesh"], "contour_factor", contour);
    mMesh->SetToonRender(useToon);
    mMesh->SetContourFactor(contour);
    mMesh->SetContourColor(Vector3(0.2f, 0.2f, 0.2f));

    //------------------------------------------------------------------
    // Transform
    //------------------------------------------------------------------
    Vector3 pos;
    JsonHelper::GetVector3(json, "position", pos);
    SetPosition(pos);

    Quaternion q;
    JsonHelper::GetQuaternionFromEuler(json, "rotation_deg", q);
    SetRotation(q);

    float scale = 1.0f;
    JsonHelper::GetFloat(json, "scale", scale);
    mMesh->SetLocalScale(scale);

    //------------------------------------------------------------------
    // コライダー
    //------------------------------------------------------------------
    Vector3 vOffset, vScale;
    JsonHelper::GetVector3(json["collider"], "bounding_box_offset", vOffset);
    JsonHelper::GetVector3(json["collider"], "bounding_box_scale",  vScale);
    SetupCollider(mMesh,
                  toy::C_FOOT | toy::C_BODY | toy::C_GROUND | toy::C_WALL | toy::C_PLAYER_TEAM,
                  vOffset, vScale);

    //------------------------------------------------------------------
    // 重力
    //------------------------------------------------------------------
    auto* grav = SetupGravity();
    grav->SetEnableGroundPose(false);
    grav->SetGravityAccel(-80.0f);
    grav->SetJumpSpeed(35.0f);

    //------------------------------------------------------------------
    // 移動 / カメラ
    //  ロックオンが必要な場合は SetupSensor() を追加で呼ぶ
    //------------------------------------------------------------------
    SetupMove();
    SetupCamera();
    SetupSensor();  // ロックオン有効（L1/R1 で切替、L2+ボタンで攻撃）

    //------------------------------------------------------------------
    // 足音
    //------------------------------------------------------------------
    mSound = CreateComponent<toy::SoundComponent>();
    mSound->SetSound("Walk.wav");
    mSound->SetVolume(0.5f);
    mSound->Enable3DSound(true);

    //------------------------------------------------------------------
    // 魔法アクター
    //------------------------------------------------------------------
    mMagic = GetApp()->CreateActor<MagicActor>();
    mHeal  = GetApp()->CreateActor<HealMagicActor>();
}

//-----------------------------------------------------------------------------
// OnAttackInput（L2 押下中に呼ばれる）
//-----------------------------------------------------------------------------
void HeroActor::OnAttackInput(const toy::InputState& state)
{
    if (!IsMovable()) return;

    auto* anim = mMesh->GetAnimPlayer();
    anim->SetPlayRate(1.5f);

    if (state.IsButtonPressed(toy::GameButton::B))
    {
        anim->PlayOnce(H_Slash, H_Stand);
        SetMovable(false);
    }
    else if (state.IsButtonPressed(toy::GameButton::X))
    {
        anim->PlayOnce(H_Spin, H_Stand);
        SetMovable(false);
        mHeal->Spawn(GetPosition());
    }
    else if (state.IsButtonPressed(toy::GameButton::Y))
    {
        anim->PlayOnce(H_Stab, H_Stand);
        SetMovable(false);
        mMagic->Spawn(GetPosition(), GetForward());
    }
}

//-----------------------------------------------------------------------------
// UpdatePlayerAnim（毎フレーム）
//-----------------------------------------------------------------------------
void HeroActor::UpdatePlayerAnim(float /*deltaTime*/)
{
    auto* anim = mMesh->GetAnimPlayer();
    anim->SetPlayRate(1.5f);

    if (!IsMovable())
    {
        // 攻撃終了（ループ or 完了）で移動解除
        if (anim->IsLooping() || anim->IsFinished())
        {
            SetMovable(true);
        }
        return;
    }

    auto* move = GetMoveComp();
    const bool inAir = (mGravity && mGravity->GetVelocityY() != 0.0f);

    if (inAir)
    {
        anim->Play(H_Jump);
        if (mSound) mSound->Stop();
    }
    else if (move &&
             move->GetForwardSpeed() == 0.0f &&
             move->GetRightSpeed()   == 0.0f &&
             move->GetAngularSpeed() == 0.0f)
    {
        anim->Play(H_Stand);
        if (mSound) mSound->Stop();
    }
    else
    {
        // バトルモード（ロックオン中）はストレイフ用アニメ
        const int moveMotion = IsInBattle() ? H_WalkSS : H_Run;
        anim->Play(moveMotion);
        if (mSound && !mSound->IsPlaying()) mSound->Play();
    }
}

//-----------------------------------------------------------------------------
// OnPlayerInput（ジャンプなど）
//-----------------------------------------------------------------------------
void HeroActor::OnPlayerInput(const toy::InputState& state)
{
    if (!IsMovable()) return;

    if (state.IsButtonPressed(toy::GameButton::A))
    {
        if (mGravity) mGravity->Jump();
        mMesh->GetAnimPlayer()->PlayOnce(H_Jump, H_Stand);
    }
}
