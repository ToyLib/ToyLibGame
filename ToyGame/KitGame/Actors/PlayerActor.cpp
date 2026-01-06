#include "PlayerActor.h"
#include "ToyLib.h"
#include <iostream>

// JSON Helper などは既存のものを利用
// #include "JsonHelper.h" など必要なら追加

PlayerActor::PlayerActor(toy::Application* a)
    : Actor(a)
    , mAnimID(H_Stand)
    , mPlayMode(PlayMode::Field)
    , mMovable(true)
    , mInputAttack(false)
    , mSelectedTarget(NO_TARGET)
    , mTargetCollider(nullptr)
    , mMoveComp(nullptr)
    , mDirMove(nullptr)
    , mFPSMove(nullptr)
    , mOrbitMove(nullptr)
    , mMeshComp(nullptr)
    , mCollComp(nullptr)
    , mCameraComp(nullptr)
    , mGravComp(nullptr)
    , mSound(nullptr)
    , mSensor(nullptr)
{
    // --- JSON読み込み ---
    std::ifstream file("ToyGame/Settings/KitHero.json");
    nlohmann::json json;
    file >> json;

    // --- スケルタルメッシュ ---
    mMeshComp = CreateComponent<toy::SkeletalMeshComponent>();
    std::string meshPath;
    if (json.contains("mesh") && json["mesh"].contains("file"))
    {
        JsonHelper::GetString(json["mesh"], "file", meshPath);
    }
    mMeshComp->SetMesh(a->GetAssetManager()->GetMesh(meshPath));

    bool  useToon = false;
    float contour = 1.00f;
    JsonHelper::GetBool(json["mesh"], "toon_render", useToon);
    JsonHelper::GetFloat(json["mesh"], "contour_factor", contour);
    mMeshComp->SetToonRender(useToon);
    mMeshComp->SetContourFactor(contour);
    mMeshComp->SetContourColor(Vector3(0.2f, 0.2f, 0.2f));
    mMeshComp->SetYawOffset(Math::ToRadians(180.0f));

    // --- Transform設定 ---
    Vector3 pos;
    JsonHelper::GetVector3(json, "position", pos);
    SetPosition(pos);

    Quaternion q;
    JsonHelper::GetQuaternionFromEuler(json, "rotation_deg", q);
    SetRotation(q);

    float scale = 1.0f;
    JsonHelper::GetFloat(json, "scale", scale);
    mMeshComp->SetLocalScale(scale);

    // --- コライダー ---
    mCollComp = CreateComponent<toy::ColliderComponent>();
    mCollComp->GetBoundingVolume()->ComputeFromMeshComponent(mMeshComp);
    Vector3 vOffset;
    JsonHelper::GetVector3(json["collider"], "bounding_box_offset", vOffset);
    Vector3 vScale;
    JsonHelper::GetVector3(json["collider"], "bounding_box_scale", vScale);
    mCollComp->GetBoundingVolume()->AdjustBoundingBox(vOffset, vScale);
    mCollComp->SetFlags(toy::C_FOOT | toy::C_PLAYER_TEAM);
    mCollComp->SetEnabled(true);

    // --- 移動コンポーネント ---
    mDirMove   = CreateComponent<toy::DirMoveComponent>();
    mOrbitMove = CreateComponent<toy::OrbitMoveComponent>();
    // FPSMove は今は未使用なら nullptr のままでもOK
    // mFPSMove = CreateComponent<toy::FPSMoveComponent>();

    // 初期はフィールド移動
    mMoveComp = mDirMove;
    mDirMove->SetIsMovable(true);
    mOrbitMove->SetIsMovable(false);

    // --- カメラコンポーネント ---
    mFollowCamera = CreateComponent<toy::FollowCameraComponent>();
    mOrbitCamera = CreateComponent<toy::OrbitCameraComponent>();
    mOrbitCamera->SetIsEnable(false);

    // --- 重力コンポーネント ---
    mGravComp = CreateComponent<toy::GravityComponent>();
    mGravComp->SetEnableGroundPose(false);

    // --- センサーコンポーネント ---
    toy::SensorComponent::Desc sensorDesc =
    {
        .fovRad  = Math::ToRadians(360.0f),
        .maxDist = 30.0f
    };
    mSensor = CreateComponent<toy::SensorComponent>(sensorDesc);

    // --- サウンドコンポーネント ---
    mSound = CreateComponent<toy::SoundComponent>();
    mSound->SetSound("Hero/Walk.wav");
    mSound->SetVolume(0.5f);
    mSound->Enable3DSound(true);

    //mMagic = GetApp()->CreateActor<MagicActor>();
    //mHeal  = GetApp()->CreateActor<HealMagicActor>();
}

PlayerActor::~PlayerActor()
{
}

void PlayerActor::UpdateActor(float deltaTime)
{
    // 毎フレーム候補ターゲットを更新
    SearchTarget();

    //==============================
    // モードに応じて MoveComponent 切り替え
    //==============================
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        // バトル中＆ターゲット有り → ロックオン移動
        mDirMove->SetIsMovable(false);

        mOrbitMove->SetCenterActor(mTargetCollider->GetOwner());
        mOrbitMove->SetIsMovable(true);

        mMoveComp = mOrbitMove;
        mOrbitCamera->SetIsEnable(false);
        mFollowCamera->SetIsEnable(true);
    }
    else
    {
        // ターゲットなし → フィールドモードに戻す
        mOrbitMove->SetIsMovable(false);
        mDirMove->SetIsMovable(true);

        mMoveComp  = mDirMove;
        mPlayMode  = PlayMode::Field;
        mTargetCollider = nullptr;
        mSelectedTarget = NO_TARGET;

        mOrbitCamera->SetIsEnable(true);
        mFollowCamera->SetIsEnable(false);
        


    }

    // ここで Actor 基底の UpdateActor 内処理を呼ぶかどうかは
    // エンジン側の仕様次第（必要なら super 呼び）
    // toy::Actor::UpdateActor(deltaTime); // 必要なら
}

void PlayerActor::SearchTarget()
{
    auto candidate = mSensor->GetHits();

    mCandidates.clear();

    // 画面内にあるものだけをX座標でソート
    for (auto& c : candidate)
    {
        Vector3 pos   = c.collider->GetCenterPosition();
        auto    scInfo = GetApp()->GetRenderer()->WorldToScreen(pos);
        if (!scInfo.visible)
        {
            continue;
        }

        TargetInfo info;
        info.collider  = c.collider;
        info.screenPos = scInfo.virtualScreen;

        auto itr = mCandidates.begin();
        for (; itr != mCandidates.end(); ++itr)
        {
            if (info.screenPos.x < itr->screenPos.x)
            {
                break;
            }
        }
        mCandidates.insert(itr, info);
    }

    // 既にロック中のターゲットが候補に残っているかチェック
    mSelectedTarget = NO_TARGET;
    for (int i = 0; i < static_cast<int>(mCandidates.size()); ++i)
    {
        if (mCandidates[i].collider == mTargetCollider)
        {
            mTargetCollider->SetTargetState(toy::TargetState::Locked);
            mSelectedTarget = i;
            break;
        }
    }

    // ロック中ターゲットが画面内から消えた場合
    if (mSelectedTarget == NO_TARGET)
    {
        if (mTargetCollider)
        {
            EnterFieldMode();
        }
    }
}

void PlayerActor::EnterBattleMode()
{
    if (mPlayMode == PlayMode::Field)
    {
        mTargetCollider = mCandidates[mSelectedTarget].collider;
 
        
        mTargetCollider->SetTargetState(toy::TargetState::None);
        mTargetCollider = nullptr;
        mPlayMode       = PlayMode::Field;
    }
}

void PlayerActor::EnterFieldMode()
{
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::None);
        mTargetCollider = nullptr;
        mPlayMode       = PlayMode::Field;
    }
}

void PlayerActor::ActorInput(const toy::InputState& state)
{
    // まずはモードに応じた移動（速度セット → Updateで反映）
    if (mPlayMode == PlayMode::Field)
    {
        FieldMove(state);
    }
    else
    {
        BattleMove(state);
    }

    // ターゲット切り替え
    SelectTarget(state);

    // 攻撃入力（攻撃中の移動ロックなどは ApplyGroundMoveAndAnim 内で管理）
    if (state.IsButtonDown(toy::GameButton::L2))
    {
        InputAttack(state);
    }
    
    if (state.IsButtonPressed(toy::GameButton::B))
    {
        EnterFieldMode();
    }

}

void PlayerActor::SelectTarget(const toy::InputState& state)
{
    if (mCandidates.empty())
    {
        return;
    }

    if (state.IsButtonPressed(toy::GameButton::L1))
    {
        if (mSelectedTarget == NO_TARGET)
        {
            mSelectedTarget = static_cast<int>(mCandidates.size()) / 2;
        }
        if (mSelectedTarget > 0)
        {
            --mSelectedTarget;
        }
    }

    if (state.IsButtonPressed(toy::GameButton::R1))
    {
        if (mSelectedTarget == NO_TARGET)
        {
            mSelectedTarget = static_cast<int>(mCandidates.size()) / 2;
        }
        if (mSelectedTarget < static_cast<int>(mCandidates.size()) - 1)
        {
            ++mSelectedTarget;
        }
    }

    if (mSelectedTarget != NO_TARGET)
    {
        if (mTargetCollider)
        {
            mTargetCollider->SetTargetState(toy::TargetState::Candidate);
        }

        mTargetCollider = mCandidates[mSelectedTarget].collider;
        mTargetCollider->SetTargetState(toy::TargetState::Locked);

        mPlayMode = PlayMode::Battle;
    }
}

void PlayerActor::InputAttack(const toy::InputState& state)
{
    if (mPlayMode != PlayMode::Battle)
    {
        return;
    }

    mInputAttack = false;

    auto* animPlayer = mMeshComp->GetAnimPlayer();
    animPlayer->SetPlayRate(1.5f);

    // --- 移動可能状態のみ攻撃を受け付ける ---
    if (mMovable)
    {
        // 攻撃入力（優先度付き）
        if (state.IsButtonPressed(toy::GameButton::B))
        {
            animPlayer->PlayOnce(H_Slash, H_Stand);
            mInputAttack = true;
        }
        else if (state.IsButtonPressed(toy::GameButton::X))
        {
            animPlayer->PlayOnce(H_Spin, H_Stand);
            mInputAttack = true;
            // mHeal->Spawn(GetPosition());
        }
        else if (state.IsButtonPressed(toy::GameButton::Y))
        {
            animPlayer->PlayOnce(H_Stab, H_Stand);
            mInputAttack = true;
            // mMagic->Spawn(GetPosition(), GetForward());
        }
        
    }

    if (mInputAttack)
    {
        // このフレーム以降、アニメ側で解除されるまで移動ロック
        mMovable = false;
    }
}

//----------------------------------------
// フィールド移動
//----------------------------------------
void PlayerActor::FieldMove(const toy::InputState& state)
{
    // DirMove / mMoveComp 側が入力を受けて mForwardSpeed / mRightSpeed を更新
    // （DirMove::ProcessInput が呼ばれるのはエンジン側の責務）

    // フィールド時は「走り」モーション
    ApplyGroundMoveAndAnim(state, H_Run);
}

//----------------------------------------
// バトル移動（ロックオン移動）
//----------------------------------------
void PlayerActor::BattleMove(const toy::InputState& state)
{
    // OrbitMove / mMoveComp 側が入力を受けて mForwardSpeed / mRightSpeed を更新

    // バトル時はロックオン用の歩きアニメ（サイドステップ等）
    ApplyGroundMoveAndAnim(state, H_WalkSS);
}

//----------------------------------------
// フィールド／バトル共通の地上移動＆アニメ処理
//----------------------------------------
void PlayerActor::ApplyGroundMoveAndAnim(const toy::InputState& state,
                                         PlayerMotion moveMotion)
{
    auto* animPlayer = mMeshComp->GetAnimPlayer();
    animPlayer->SetPlayRate(1.5f);

    toy::MoveComponent* move = GetActiveMove();

    // --- 移動可能状態 ---
    if (mMovable)
    {
        if (mInputAttack)
        {
            // 攻撃入力があったフレーム以降は移動ロック
            mMovable = false;
        }
        else
        {
            // ジャンプ（移動ロックしない）
            if (state.IsButtonPressed(toy::GameButton::A))
            {
                mGravComp->Jump();
                animPlayer->PlayOnce(H_Jump, H_Stand);
            }

            // --- 状態に応じた通常アニメ切り替え ---
            if (mGravComp->GetVelocityY() != 0.0f)
            {
                // 空中は常にジャンプアニメ（移動は可能）
                animPlayer->Play(H_Jump);
            }
            else if (move->GetForwardSpeed() == 0.0f &&
                     move->GetRightSpeed()   == 0.0f &&
                     move->GetAngularSpeed() == 0.0f)
            {
                // 完全停止
                animPlayer->Play(H_Stand);
                mSound->Stop();
            }
            else
            {
                // 何かしら動いている
                animPlayer->Play(moveMotion);
                if (!mSound->IsPlaying())
                {
                    mSound->Play();
                }
            }
        }

        // 上下に速度があれば足音は止める
        if (mGravComp->GetVelocityY() != 0.0f)
        {
            mSound->Stop();
        }
    }
    else
    {
        // 攻撃終了したら解除（ループアニメ or 再生完了）
        if (animPlayer->IsLooping() || animPlayer->IsFinished())
        {
            mMovable     = true;
            mInputAttack = false;
        }
    }

    // 最後に、現在アクティブな MoveComponent に移動可否を反映
    move->SetIsMovable(mMovable);
}
