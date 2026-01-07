#include "PlayerActor.h"
#include "ToyLib.h"
#include <iostream>

// 必要なら JsonHelper 用のヘッダも
// #include "JsonHelper.h"

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
    JsonHelper::GetBool(json["mesh"],   "toon_render",    useToon);
    JsonHelper::GetFloat(json["mesh"],  "contour_factor", contour);

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
    mDirMove   = CreateComponent<toy::DirMoveComponent>();   // フィールド用
    mOrbitMove = CreateComponent<toy::OrbitMoveComponent>(); // バトル（ロックオン）用
    // mFPSMove = CreateComponent<toy::FPSMoveComponent>();  // 将来用

    // 初期はフィールド移動
    mMoveComp = mDirMove;
    mDirMove->SetIsMovable(true);
    mOrbitMove->SetIsMovable(false);

    //==================================================================
    // 6) カメラコンポーネント（フィールド / バトル）
    //==================================================================
    mFollowCamera = CreateComponent<toy::FollowCameraComponent>(); // バトル寄り視点
    mOrbitCamera  = CreateComponent<toy::OrbitCameraComponent>();  // フィールド視点

    // 初期状態はフィールドモード想定 → Orbit カメラをアクティブに
    GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);

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
        .fovRad  = Math::ToRadians(360.0f),
        .maxDist = 30.0f
    };
    mSensor = CreateComponent<toy::SensorComponent>(sensorDesc);

    //==================================================================
    // 9) サウンドコンポーネント（足音など）
    //==================================================================
    mSound = CreateComponent<toy::SoundComponent>();
    mSound->SetSound("Hero/Walk.wav");
    mSound->SetVolume(0.5f);
    mSound->Enable3DSound(true);

    // mMagic = GetApp()->CreateActor<MagicActor>();
    // mHeal  = GetApp()->CreateActor<HealMagicActor>();
}

PlayerActor::~PlayerActor() = default;

//======================================================================
// UpdateActor
//
//  ・毎フレーム、ターゲット状態を更新
//  ・PlayMode（Field / Battle）とターゲット有無に応じて
//      - MoveComponent の切り替え
//      - カメラの切り替え
//======================================================================
void PlayerActor::UpdateActor(float deltaTime)
{
    // 1) 毎フレーム候補ターゲットを更新
    SearchTarget();

    // 2) モードに応じて MoveComponent / Camera を切り替え
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        //========================
        // バトル中 ＆ ターゲットあり → ロックオン移動 + Followカメラ
        //========================
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
        //========================
        // ターゲットなし → フィールドモードに戻す
        //========================
        mOrbitMove->SetIsMovable(false);
        mDirMove->SetIsMovable(true);

        mMoveComp      = mDirMove;
        mPlayMode      = PlayMode::Field;
        mTargetCollider = nullptr;
        mSelectedTarget = NO_TARGET;

        GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
        mOrbitCamera->SetIsEnabled(true);
        mFollowCamera->SetIsEnabled(false);
    }

    // Actor 基底の UpdateActor を呼ぶかどうかはエンジン仕様次第
    // toy::Actor::UpdateActor(deltaTime);
}

//======================================================================
// SearchTarget
//
//  ・SensorComponent から検出されたヒットを取得
//  ・画面内に映っているコライダのみを候補として保持し、X 座標でソート
//  ・既にロック中のターゲットが候補内に残っているかを確認
//  ・ロック中ターゲットが画面外に出たら自動で Field モードへ戻す
//======================================================================
void PlayerActor::SearchTarget()
{
    auto candidate = mSensor->GetHits();
    mCandidates.clear();

    // 画面内にあるものだけを X 座標でソート
    for (auto& c : candidate)
    {
        Vector3 pos    = c.collider->GetCenterPosition();
        auto    scInfo = GetApp()->GetRenderer()->WorldToScreen(pos);
        if (!scInfo.visible)
        {
            continue;
        }

        TargetInfo info;
        info.collider  = c.collider;
        info.screenPos = scInfo.virtualScreen;

        // X 座標の昇順で挿入
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

    // ロック中ターゲットが画面内から消えた場合 → フィールドモードに戻る
    if (mSelectedTarget == NO_TARGET)
    {
        if (mTargetCollider)
        {
            EnterFieldMode();
        }
    }
}

//======================================================================
// EnterBattleMode
//
//  ★元コードは「ターゲットを即 None にして Field に戻す」挙動で
//    実質 no-op になっていたので、バトル突入用に整理した案
//======================================================================
void PlayerActor::EnterBattleMode()
{
    if (mPlayMode == PlayMode::Field &&
        mSelectedTarget != NO_TARGET &&
        mSelectedTarget < static_cast<int>(mCandidates.size()))
    {
        // 選択中インデックスからターゲット確定
        mTargetCollider = mCandidates[mSelectedTarget].collider;
        mTargetCollider->SetTargetState(toy::TargetState::Locked);
        mPlayMode = PlayMode::Battle;
    }
}

//======================================================================
// EnterFieldMode
//
//  ・バトル中に呼ぶことでロックを解除し、フィールドモードへ戻す
//======================================================================
void PlayerActor::EnterFieldMode()
{
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::None);
        mTargetCollider  = nullptr;
        mSelectedTarget  = NO_TARGET;
        mPlayMode        = PlayMode::Field;
    }
}

//======================================================================
// ActorInput
//
//  ・入力 → モードに応じた移動処理（FieldMove / BattleMove）
//  ・ターゲット選択（SelectTarget）
//  ・攻撃入力（InputAttack）
//  ・B でフィールドモードへ戻る
//======================================================================
void PlayerActor::ActorInput(const toy::InputState& state)
{
    // 1) モードに応じた移動入力処理
    if (mPlayMode == PlayMode::Field)
    {
        FieldMove(state);
    }
    else
    {
        BattleMove(state);
    }

    // 2) ターゲット切り替え（L1 / R1）
    SelectTarget(state);

    // 3) 攻撃入力
    if (state.IsButtonDown(toy::GameButton::L2))
    {
        InputAttack(state);
    }

    // 4) B でフィールドモードに戻る
    if (state.IsButtonPressed(toy::GameButton::B))
    {
        EnterFieldMode();
    }
}

//======================================================================
// SelectTarget
//
//  ・画面内候補 mCandidates から L1 / R1 でターゲット選択
//  ・選択されたターゲットに Locked 状態を付与
//  ・ターゲットが選ばれた時点で PlayMode を Battle に切り替え
//======================================================================
void PlayerActor::SelectTarget(const toy::InputState& state)
{
    if (mCandidates.empty())
    {
        return;
    }

    // L1：左のターゲットへ
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

    // R1：右のターゲットへ
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
        // 以前のターゲットがあれば Candidate に戻す
        if (mTargetCollider)
        {
            mTargetCollider->SetTargetState(toy::TargetState::Candidate);
        }

        // 新たに選ばれたターゲットに Locked を付与
        mTargetCollider = mCandidates[mSelectedTarget].collider;
        mTargetCollider->SetTargetState(toy::TargetState::Locked);

        // バトルモードへ移行（実際のカメラ/Move 切替は UpdateActor 側）
        mPlayMode = PlayMode::Battle;
    }
}

//======================================================================
// InputAttack
//
//  ・バトルモード時のみ攻撃入力を受け付ける
//  ・攻撃開始時にアニメ再生＋移動ロック開始
//  ・移動ロック解除は ApplyGroundMoveAndAnim 内で管理
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

    // 移動可能状態のみ攻撃を受け付ける
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
    // DirMove / mMoveComp 側が入力を受けて速度更新
    // フィールド時は「走り」モーションを基本とする
    ApplyGroundMoveAndAnim(state, H_Run);
}

//----------------------------------------
// バトル移動（ロックオン移動）
//----------------------------------------
void PlayerActor::BattleMove(const toy::InputState& state)
{
    // OrbitMove / mMoveComp 側が入力を受けて速度更新
    // バトル時はロックオン用の歩きアニメ（サイドステップ等）
    ApplyGroundMoveAndAnim(state, H_WalkSS);
}

//======================================================================
// ApplyGroundMoveAndAnim
//
//  ・フィールド／バトル共通の地上移動＆アニメ制御
//  ・責務：
//      - ジャンプ／落下／待機／歩き／走りアニメの切り替え
//      - 足音の ON/OFF
//      - 攻撃による移動ロック管理
//      - 最終的な「移動可否」を MoveComponent に反映
//======================================================================
void PlayerActor::ApplyGroundMoveAndAnim(const toy::InputState& state,
                                         PlayerMotion moveMotion)
{
    auto* animPlayer = mMeshComp->GetAnimPlayer();
    animPlayer->SetPlayRate(1.5f);

    toy::MoveComponent* move = GetActiveMove();

    //========================
    // 移動可能状態
    //========================
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

            //--- 状態に応じた通常アニメ切り替え ---
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

        // 上下方向に速度がある間は足音は止める
        if (mGravComp->GetVelocityY() != 0.0f)
        {
            mSound->Stop();
        }
    }
    //========================
    // 移動ロック中（攻撃中）
    //========================
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
