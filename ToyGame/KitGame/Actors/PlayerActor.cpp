#include "PlayerActor.h"
#include "ToyLib.h"

#include <fstream>
#include <iostream>

// JsonHelper / nlohmann::json を使ってる想定
// #include "JsonHelper.h"

PlayerActor::PlayerActor(toy::Application* a)
    : Actor(a)
    , mAnimID(H_Stand)
    , mPlayMode(PlayMode::Field)
    , mPrevPlayMode(PlayMode::Field)   // ★追加
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
    , mOrbitCamera(nullptr)
    , mFollowCamera(nullptr)
    , mGravComp(nullptr)
    , mSound(nullptr)
    , mSensor(nullptr)
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

    mMoveComp = mDirMove;
    mDirMove->SetIsMovable(true);
    mOrbitMove->SetIsMovable(false);

    //==================================================================
    // 6) カメラコンポーネント（フィールド / バトル）
    //==================================================================
    mFollowCamera = CreateComponent<toy::FollowCameraComponent>();
    mOrbitCamera  = CreateComponent<toy::OrbitCameraComponent>();

    // 初期状態はフィールド想定
    GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
    mOrbitCamera->SetIsEnabled(true);
    mFollowCamera->SetIsEnabled(false);

    //==================================================================
    // 7) 重力コンポーネント
    //==================================================================
    mGravComp = CreateComponent<toy::GravityComponent>();
    mGravComp->SetEnableGroundPose(false);

    //==================================================================
    // 8) センサーコンポーネント（ターゲット候補検出）
    //==================================================================
    toy::SensorComponent::Desc sensorDesc =
    {
        .fovRad      = Math::ToRadians(360.0f),
        .maxDist     = 30.0f,
        .requireLOS  = false
    };
    mSensor = CreateComponent<toy::SensorComponent>(sensorDesc);

    //==================================================================
    // 9) サウンドコンポーネント（足音など）
    //==================================================================
    mSound = CreateComponent<toy::SoundComponent>();
    mSound->SetSound("Hero/Walk.wav");
    mSound->SetVolume(0.5f);
    mSound->Enable3DSound(true);
}

PlayerActor::~PlayerActor() = default;

//======================================================================
// UpdateActor（状態遷移を安定化）
//  - SearchTarget は毎フレームOK
//  - カメラ/Move 切替は「モードが変わった瞬間だけ」
//  - Battle中は orbitMove の中心更新だけ毎フレームやる
//======================================================================
void PlayerActor::UpdateActor(float deltaTime)
{
    // 1) 毎フレーム候補更新
    SearchTarget();

    // 2) “望ましいモード” を決定（ターゲット有無）
    PlayMode desired = (mTargetCollider != nullptr) ? PlayMode::Battle : PlayMode::Field;

    // 3) モードが変わった瞬間だけ副作用を実行
    if (desired != mPlayMode)
    {
        if (desired == PlayMode::Battle)  EnterBattleMode();
        else                              EnterFieldMode();
    }

    // 4) Battle維持処理（中心は更新してOK）
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        mOrbitMove->SetCenterActor(mTargetCollider->GetOwner());
    }

    mPrevPlayMode = mPlayMode;
}

//======================================================================
// SearchTarget（状態遷移の暴れを抑える）
//  - visibleチェックを復活（候補が暴れない）
//  - ロック中ターゲットを見失っても「攻撃中は解除しない」
//======================================================================
void PlayerActor::SearchTarget()
{
    auto hits = mSensor->GetHits();
    mCandidates.clear();

    for (auto& h : hits)
    {
        Vector3 pos    = h.collider->GetCenterPosition();
        auto    scInfo = GetApp()->GetRenderer()->WorldToScreen(pos);

        // ★ visible を復活（これがないと候補が増減してロックが揺れやすい）
        if (!scInfo.visible)
        {
            continue;
        }

        TargetInfo info;
        info.collider  = h.collider;
        info.screenPos = scInfo.virtualScreen;

        // X昇順で挿入
        auto itr = mCandidates.begin();
        for (; itr != mCandidates.end(); ++itr)
        {
            if (info.screenPos.x < itr->screenPos.x) break;
        }
        mCandidates.insert(itr, info);
    }

    // ロック中ターゲットが候補内にあるか
    int foundIndex = NO_TARGET;
    for (int i = 0; i < (int)mCandidates.size(); ++i)
    {
        if (mCandidates[i].collider == mTargetCollider)
        {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex != NO_TARGET)
    {
        mSelectedTarget = foundIndex;
        if (mTargetCollider)
        {
            mTargetCollider->SetTargetState(toy::TargetState::Locked);
        }
        return;
    }

    // ロック中ターゲットを見失った
    if (mTargetCollider)
    {
        // ★攻撃中（mMovable==false）の間は勝手に Field に戻さない
        if (mMovable)
        {
            EnterFieldMode();
        }
        else
        {
            // 何もしない：攻撃終了後に状況が戻る可能性がある
        }
    }
}

//======================================================================
// EnterBattleMode（Battleに入る瞬間だけ呼ばれる）
//======================================================================
void PlayerActor::EnterBattleMode()
{
    if (!mTargetCollider)
    {
        return;
    }

    mPlayMode = PlayMode::Battle;

    // Move 切替（1回だけ）
    mDirMove->SetIsMovable(false);

    mOrbitMove->SetCenterActor(mTargetCollider->GetOwner());
    mOrbitMove->SetIsMovable(true);
    mMoveComp = mOrbitMove;

    // Camera 切替（1回だけ）
    GetApp()->GetCameraManager()->SetActiveCamera(mFollowCamera);
    mOrbitCamera->SetIsEnabled(false);
    mFollowCamera->SetIsEnabled(true);

    // Target state
    mTargetCollider->SetTargetState(toy::TargetState::Locked);
}

//======================================================================
// EnterFieldMode（Fieldに戻る瞬間だけ呼ばれる）
//======================================================================
void PlayerActor::EnterFieldMode()
{
    // Target state を確実に解除
    if (mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::Candidate);
    }

    mTargetCollider  = nullptr;
    mSelectedTarget  = NO_TARGET;
    mPlayMode        = PlayMode::Field;

    // Move 切替（1回だけ）
    mOrbitMove->SetIsMovable(false);
    mDirMove->SetIsMovable(true);
    mMoveComp = mDirMove;

    // Camera 切替（1回だけ）
    GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
    mOrbitCamera->SetIsEnabled(true);
    mFollowCamera->SetIsEnabled(false);
}

//======================================================================
// ActorInput（状態そのものは基本触らない：ターゲット差し替え/攻撃開始だけ）
//======================================================================
void PlayerActor::ActorInput(const toy::InputState& state)
{
    // 1) モードに応じた移動入力処理（内容はそのまま）
    if (mPlayMode == PlayMode::Field)
    {
        FieldMove(state);
    }
    else
    {
        BattleMove(state);
    }

    // 2) ターゲット切り替え（L1/R1）
    SelectTarget(state);

    // 3) 攻撃入力（L2を押してる間だけ）
    if (state.IsButtonDown(toy::GameButton::L2))
    {
        InputAttack(state);
    }

    // 4) B でフィールドモードに戻る（攻撃キーと競合しないようガード）
    if (state.IsButtonPressed(toy::GameButton::B) && !state.IsButtonDown(toy::GameButton::L2))
    {
        EnterFieldMode();
    }
}

//======================================================================
// SelectTarget（ターゲット差し替えに集中：mPlayModeはここで触らない）
//======================================================================
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
            mSelectedTarget = (int)mCandidates.size() / 2;
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

    // ★ここでは mPlayMode を触らない
    //   UpdateActor が desired を見て Battle に切り替える
}

//======================================================================
// InputAttack（中身は基本そのまま）
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
        mMovable = false; // 攻撃中ロック
    }
}

//----------------------------------------
// フィールド移動（既存のまま）
//----------------------------------------
void PlayerActor::FieldMove(const toy::InputState& state)
{
    ApplyGroundMoveAndAnim(state, H_Run);
}

//----------------------------------------
// バトル移動（既存のまま）
//----------------------------------------
void PlayerActor::BattleMove(const toy::InputState& state)
{
    ApplyGroundMoveAndAnim(state, H_WalkSS);
}

//======================================================================
// ApplyGroundMoveAndAnim（既存のまま）
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
