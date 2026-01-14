#include "PlayerActor.h"
#include "ToyLib.h"

#include <fstream>
#include <iostream>
#include <cmath> // fmod, sqrt

PlayerActor::PlayerActor(toy::Application* a)
    : Actor(a)
{
    //==================================================================
    // 1) JSON 設定読み込み
    //==================================================================
    std::ifstream file("ToyGame/Settings/KitHero.json");
    nlohmann::json json;
    file >> json;

    //==================================================================
    // 2) スケルタルメッシュ
    //==================================================================
    mMeshComp = CreateComponent<toy::SkeletalMeshComponent>();

    std::string meshPath;
    if (json.contains("mesh") && json["mesh"].contains("file"))
    {
        JsonHelper::GetString(json["mesh"], "file", meshPath);
    }
    mMeshComp->SetMesh(a->GetAssetManager()->GetMesh(meshPath));

    bool  useToon = false;
    float contour = 1.00f;
    JsonHelper::GetBool(json["mesh"],  "toon_render",    useToon);
    JsonHelper::GetFloat(json["mesh"], "contour_factor", contour);

    mMeshComp->SetToonRender(useToon);
    mMeshComp->SetContourFactor(contour);
    mMeshComp->SetContourColor(Vector3(0.2f, 0.2f, 0.2f));
    mMeshComp->SetYawOffset(Math::ToRadians(180.0f));

    //==================================================================
    // 3) Transform 初期値
    //==================================================================
    Vector3 pos;
    JsonHelper::GetVector3(json, "position", pos);
    SetPosition(pos);

    Quaternion q;
    JsonHelper::GetQuaternionFromEuler(json, "rotation_deg", q);
    SetRotation(q);

    float scale = 1.0f;
    JsonHelper::GetFloat(json, "scale", scale);
    mMeshComp->SetLocalScale(scale);

    //==================================================================
    // 4) コライダー（足元＋プレイヤーチーム）
    //==================================================================
    mCollComp = CreateComponent<toy::ColliderComponent>();
    mCollComp->GetBoundingVolume()->ComputeFromMeshComponent(mMeshComp);

    Vector3 vOffset;
    Vector3 vScale;
    JsonHelper::GetVector3(json["collider"], "bounding_box_offset", vOffset);
    JsonHelper::GetVector3(json["collider"], "bounding_box_scale",  vScale);
    mCollComp->GetBoundingVolume()->AdjustBoundingBox(vOffset, vScale);

    mCollComp->SetFlags(toy::C_FOOT | toy::C_PLAYER_TEAM);
    mCollComp->SetEnabled(true);

    //==================================================================
    // 5) 移動コンポーネント（フィールド移動 / ロックオン移動）
    //==================================================================
    mDirMove   = CreateComponent<toy::DirMoveComponent>();
    mOrbitMove = CreateComponent<toy::OrbitMoveComponent>();
    // mFPSMove = CreateComponent<toy::FPSMoveComponent>(); // 将来用

    // 初期はフィールド移動
    mMoveComp = mDirMove;
    mDirMove->SetIsMovable(true);
    mOrbitMove->SetIsMovable(false);

    //==================================================================
    // 6) カメラコンポーネント（フィールド / バトル）
    //==================================================================
    mFollowCamera = CreateComponent<toy::FollowCameraComponent>();
    mOrbitCamera  = CreateComponent<toy::OrbitCameraComponent>();

    GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
    mOrbitCamera->SetIsEnabled(true);
    mFollowCamera->SetIsEnabled(false);

    //==================================================================
    // 7) 重力コンポーネント
    //==================================================================
    mGravComp = CreateComponent<toy::GravityComponent>();
    mGravComp->SetEnableGroundPose(false);

    //==================================================================
    // 8) センサーコンポーネント
    //==================================================================
    toy::SensorComponent::Desc sensorDesc =
    {
        .fovRad      = Math::ToRadians(360.0f),
        .maxDist     = 30.0f,
        .requireLOS  = false
    };
    mSensor = CreateComponent<toy::SensorComponent>(sensorDesc);

    //==================================================================
    // 9) サウンド
    //==================================================================
    mSound = CreateComponent<toy::SoundComponent>();
    mSound->SetSound("Hero/Walk.wav");
    mSound->SetVolume(0.5f);
    mSound->Enable3DSound(true);
}

PlayerActor::~PlayerActor() = default;

//======================================================================
// UpdateActor（元ロジック）
//  - SearchTargetは毎フレーム
//  - PlayMode と Target有無で Move/Camera を決める
//======================================================================
void PlayerActor::UpdateActor(float deltaTime)
{
    // 1) 候補更新
    SearchTarget(deltaTime);

    // 2) モードに応じた Move / Camera 切替（元コードの分岐に戻す）
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        // Battle中 & ターゲットあり
        mDirMove->SetIsMovable(false);

        mOrbitMove->SetCenterActor(mTargetCollider->GetOwner());
        mOrbitMove->SetIsMovable(true);
        mMoveComp = mOrbitMove;

        GetApp()->GetCameraManager()->SetActiveCamera(mFollowCamera);
        mOrbitCamera->SetIsEnabled(false);
        mFollowCamera->SetIsEnabled(true);
    }
    else
    {
        // ターゲットなし or Battleじゃない → Fieldへ戻す
        // ※ここで「強制的にFieldへ戻す」のが元ロジック
        mOrbitMove->SetIsMovable(false);
        mDirMove->SetIsMovable(true);

        mMoveComp       = mDirMove;
        mPlayMode       = PlayMode::Field;
        mTargetCollider = nullptr;
        mSelectedTarget = NO_TARGET;
        mLockLostTime   = 0.0f;

        GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
        mOrbitCamera->SetIsEnabled(true);
        mFollowCamera->SetIsEnabled(false);
    }
}

//======================================================================
// SearchTarget（元ロジック）
//  - センサーHits → mCandidates作成
//  - X挿入ソート（元方式）
//  - ロック中が候補に残っているかを見る
//  - RPG寄り：見失い猶予 + 距離解除（ここだけ追加）
//======================================================================
void PlayerActor::SearchTarget(float deltaTime)
{
    auto hits = mSensor->GetHits();
    mCandidates.clear();

    // 1) 候補作成（元：挿入でX昇順にする）
    for (auto& h : hits)
    {
        auto* col = h.collider;
        if (!col) continue;

        Vector3 pos    = col->GetCenterPosition();
        auto    scInfo = GetApp()->GetRenderer()->WorldToScreen(pos);

        TargetInfo info;
        info.collider  = col;
        info.screenPos = scInfo.virtualScreen;

        // X昇順で挿入
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

    // 2) ロック中が候補内に残っているか（元ロジック：mSelectedTarget を探し直す）
    mSelectedTarget = NO_TARGET;

    for (int i = 0; i < (int)mCandidates.size(); ++i)
    {
        if (mCandidates[i].collider == mTargetCollider)
        {
            mTargetCollider->SetTargetState(toy::TargetState::Locked);
            mSelectedTarget = i;
            break;
        }
    }

    // 3) ロックが無いなら猶予タイマはリセット（元ロジックの自然な追加）
    if (!mTargetCollider)
    {
        mLockLostTime = 0.0f;
        return;
    }

    // 4) RPG寄り：解除条件（猶予 + 距離）
    const bool inAttackLock = !mMovable;

    // 距離解除（sqrt回避）
    const Vector3 d = (mTargetCollider->GetOwner()->GetPosition() - GetPosition());
    const float distSq = d.x*d.x + d.y*d.y + d.z*d.z;
    const float breakSq = mLockBreakDist * mLockBreakDist;
    const bool tooFar = (distSq > breakSq);

    const bool stillInCandidates = (mSelectedTarget != NO_TARGET);

    if (stillInCandidates)
    {
        mLockLostTime = 0.0f;
    }
    else
    {
        if (!inAttackLock)
        {
            mLockLostTime += deltaTime;
        }
    }

    // 解除：遠すぎ OR（攻撃中じゃなくて）猶予超え
    if (tooFar || (!inAttackLock && mLockLostTime > mLockLostGraceSec))
    {
        EnterFieldMode();
        mLockLostTime = 0.0f;
    }
}

//======================================================================
// EnterBattleMode（元ロジック）
//  - 選択中インデックスからターゲット確定してBattleへ
//======================================================================
void PlayerActor::EnterBattleMode()
{
    if (mPlayMode == PlayMode::Field &&
        mSelectedTarget != NO_TARGET &&
        mSelectedTarget < (int)mCandidates.size())
    {
        mTargetCollider = mCandidates[mSelectedTarget].collider;
        if (mTargetCollider)
        {
            mTargetCollider->SetTargetState(toy::TargetState::Locked);
        }
        mPlayMode = PlayMode::Battle;
    }
}

//======================================================================
// EnterFieldMode（元ロジック）
//  - ロック解除してFieldへ
//  - ★Move/Cameraの復帰は UpdateActor の else 側がやる（元方式）
//======================================================================
void PlayerActor::EnterFieldMode()
{
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::Candidate);
        mTargetCollider = nullptr;
        mSelectedTarget = NO_TARGET;
        mPlayMode       = PlayMode::Field;
        mLockLostTime   = 0.0f;
    }
}

//======================================================================
// ActorInput（元ロジック）
//======================================================================
void PlayerActor::ActorInput(const toy::InputState& state)
{
    // 移動
    if (mPlayMode == PlayMode::Field)
    {
        FieldMove(state);
    }
    else
    {
        BattleMove(state);
    }

    // ターゲット選択（L1/R1）
    SelectTarget(state);

    // 攻撃入力（L2押下中）
    if (state.IsButtonDown(toy::GameButton::L2))
    {
        InputAttack(state);
    }

    // Bでロック解除（L2と競合しない）
    if (state.IsButtonPressed(toy::GameButton::B) && !state.IsButtonDown(toy::GameButton::L2))
    {
        EnterFieldMode();
    }
}

//======================================================================
// SelectTarget（元ロジック）
//  - mSelectedTarget を L1/R1で動かす
//  - mTargetCollider を差し替え
//  - Battleへ移行
//======================================================================
void PlayerActor::SelectTarget(const toy::InputState& state)
{
    if (mCandidates.empty())
    {
        return;
    }

    // L1：左
    if (state.IsButtonPressed(toy::GameButton::L1))
    {
        if (mSelectedTarget == NO_TARGET)
        {
            mSelectedTarget = (int)mCandidates.size() / 2;
        }
        if (mSelectedTarget > 0)
        {
            --mSelectedTarget;
        }
    }

    // R1：右
    if (state.IsButtonPressed(toy::GameButton::R1))
    {
        if (mSelectedTarget == NO_TARGET)
        {
            mSelectedTarget = (int)mCandidates.size() / 2;
        }
        if (mSelectedTarget < (int)mCandidates.size() - 1)
        {
            ++mSelectedTarget;
        }

    }

    if (mSelectedTarget == NO_TARGET)
    {
        return;
    }

    // 前ターゲットを Candidate に戻す
    if (mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::Candidate);
    }

    // 新ターゲットを Locked
    mTargetCollider = mCandidates[mSelectedTarget].collider;
    if (mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::Locked);
    }

    // Battleへ（切替副作用は UpdateActor が担当）
    mPlayMode     = PlayMode::Battle;
    mLockLostTime = 0.0f;
}

//======================================================================
// InputAttack（元ロジック）
//======================================================================
void PlayerActor::InputAttack(const toy::InputState& state)
{
    if (mPlayMode != PlayMode::Battle)
    {
        return;
    }

    mInputAttack = false;

    auto* animPlayer = mMeshComp->GetAnimPlayer();
    animPlayer->SetPlayRate(1.5f);

    if (mMovable)
    {
        if (state.IsButtonPressed(toy::GameButton::B))
        {
            animPlayer->PlayOnce(H_Slash, H_Stand);
            mInputAttack = true;
        }
        else if (state.IsButtonPressed(toy::GameButton::X))
        {
            animPlayer->PlayOnce(H_Spin, H_Stand);
            mInputAttack = true;
        }
        else if (state.IsButtonPressed(toy::GameButton::Y))
        {
            animPlayer->PlayOnce(H_Stab, H_Stand);
            mInputAttack = true;
        }
    }

    if (mInputAttack)
    {
        mMovable = false;
    }
}

//----------------------------------------
// フィールド移動
//----------------------------------------
void PlayerActor::FieldMove(const toy::InputState& state)
{
    ApplyGroundMoveAndAnim(state, H_Run);
}

//----------------------------------------
// バトル移動
//----------------------------------------
void PlayerActor::BattleMove(const toy::InputState& state)
{
    ApplyGroundMoveAndAnim(state, H_WalkSS);
}

//======================================================================
// ApplyGroundMoveAndAnim（元ロジック）
//======================================================================
void PlayerActor::ApplyGroundMoveAndAnim(const toy::InputState& state,
                                         PlayerMotion moveMotion)
{
    auto* animPlayer = mMeshComp->GetAnimPlayer();
    animPlayer->SetPlayRate(1.5f);

    toy::MoveComponent* move = GetActiveMove();

    if (mMovable)
    {
        if (mInputAttack)
        {
            mMovable = false;
        }
        else
        {
            if (state.IsButtonPressed(toy::GameButton::A))
            {
                mGravComp->Jump();
                animPlayer->PlayOnce(H_Jump, H_Stand);
            }

            if (mGravComp->GetVelocityY() != 0.0f)
            {
                animPlayer->Play(H_Jump);
            }
            else if (move->GetForwardSpeed() == 0.0f &&
                     move->GetRightSpeed()   == 0.0f &&
                     move->GetAngularSpeed() == 0.0f)
            {
                animPlayer->Play(H_Stand);
                mSound->Stop();
            }
            else
            {
                animPlayer->Play(moveMotion);
                if (!mSound->IsPlaying())
                {
                    mSound->Play();
                }
            }
        }

        if (mGravComp->GetVelocityY() != 0.0f)
        {
            mSound->Stop();
        }
    }
    else
    {
        if (animPlayer->IsLooping() || animPlayer->IsFinished())
        {
            mMovable     = true;
            mInputAttack = false;
        }
    }

    move->SetIsMovable(mMovable);
}
