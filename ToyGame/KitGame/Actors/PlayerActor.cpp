#include "PlayerActor.h"
#include "ToyLib.h"

#include <fstream>
#include <iostream>
#include <cmath> // isfinite, fabs

//======================================================================
// PlayerActor
//  - フィールド移動 + ロックオン戦闘の切替を持つプレイヤーActor
//  - ターゲット選択は「候補を画面Xで並べる」方式（元ロジック）
//  - WorldToScreen の失敗値対策と、ロック解除の猶予/距離だけ追加
//======================================================================

PlayerActor::PlayerActor(toy::Application* a)
    : Actor(a)
    , mAnimID(H_Stand)
    , mPlayMode(PlayMode::Field)
    , mMovable(true)
    , mInputAttack(false)
    , mSelectedTarget(NO_TARGET)
    , mLockLostTime(0.0f)       // 見失い累積時間
    , mLockLostGraceSec(0.7f)   // 見失い猶予
    , mLockBreakDist(35.0f)     // 距離による解除
{
    //==================================================================
    // 1) JSON 設定読み込み
    //   - 見た目/初期Transform/コライダー調整値などを外部化
    //==================================================================
    std::ifstream file("ToyGame/Settings/KitHero.json");
    nlohmann::json json;
    file >> json;

    //==================================================================
    // 2) スケルタルメッシュ
    //   - Toon/輪郭/YawOffset など見た目パラメータもここで設定
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
    // 4) コライダー
    //   - 足元判定 + プレイヤーチームとして登録
    //   - BoundingBox は Mesh から生成して、JSONの offset/scale で調整
    //==================================================================
    mCollComp = CreateComponent<toy::ColliderComponent>();
    mCollComp->GetBoundingVolume()->ComputeFromMeshComponent(mMeshComp);

    Vector3 vOffset;
    Vector3 vScale;
    JsonHelper::GetVector3(json["collider"], "bounding_box_offset", vOffset);
    JsonHelper::GetVector3(json["collider"], "bounding_box_scale",  vScale);
    mCollComp->GetBoundingVolume()->AdjustBoundingBox(vOffset, vScale);

    mCollComp->SetFlags(toy::C_FOOT | toy::C_BODY | toy::C_PLAYER_TEAM);
    mCollComp->SetEnabled(true);

    //==================================================================
    // 5) MoveComponent
    //   - Field: DirMove（通常移動）
    //   - Battle: OrbitMove（ロックオン移動）
    //   - 切替の実体は UpdateActor 側で行う（元ロジック）
    //==================================================================
    mDirMove   = CreateComponent<toy::DirMoveComponent>();
    mOrbitMove = CreateComponent<toy::OrbitMoveComponent>();
    // mFPSMove = CreateComponent<toy::FPSMoveComponent>(); // 将来用

    // 初期は Field（DirMove）
    mMoveComp = mDirMove;
    mDirMove->SetIsMovable(true);
    mOrbitMove->SetIsMovable(false);

    //==================================================================
    // 6) CameraComponent
    //   - Field: OrbitCamera
    //   - Battle: FollowCamera
    //   - ActiveCamera の切替は UpdateActor 側（元ロジック）
    //==================================================================
    mFollowCamera = CreateComponent<toy::FollowCameraComponent>();
    mOrbitCamera  = CreateComponent<toy::OrbitCameraComponent>();

    GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
    mOrbitCamera->SetIsEnabled(true);
    mOrbitCamera->SetFreezeYInAir(true);
    mFollowCamera->SetIsEnabled(false);
    mFollowCamera->SetFreezeYInAir(true);

    //==================================================================
    // 7) GravityComponent
    //   - GroundPose は無効（アニメは自前で制御）
    //==================================================================
    mGravComp = CreateComponent<toy::GravityComponent>();
    mGravComp->SetEnableGroundPose(true);

    //==================================================================
    // 8) SensorComponent
    //   - 候補検出のみ（requireLOS=false）
    //   - 候補の最終選別は「画面X並び」方式で PlayerActor が行う
    //==================================================================
    toy::SensorComponent::Desc sensorDesc =
    {
        .fovRad      = Math::ToRadians(360.0f),
        .maxDist     = 60.0f,
        .requireLOS  = false
    };
    mSensor = CreateComponent<toy::SensorComponent>(sensorDesc);

    //==================================================================
    // 9) SoundComponent
    //   - 足音（アニメ/移動状態に合わせて Play/Stop）
    //==================================================================
    mSound = CreateComponent<toy::SoundComponent>();
    mSound->SetSound("Hero/Walk.wav");
    mSound->SetVolume(0.5f);
    mSound->Enable3DSound(true);
    
    
    
    mTargetSigne = CreateComponent<toy::GroundConformSpriteComponent>();
    mTargetSigne->SetTexture(GetApp()->GetAssetManager()->GetTexture("UI/candidate.png"));
    mTargetSigne->SetSize(5, 5);
    mTargetSigne->SetBlendAdd(false);
    mTargetSigne->SetAlpha(0.8f);
    mTargetSigne->SetVisible(true);
    mTargetSigne->SetGroundLift(0.15f);
    mTargetSigne->SetGridDiv(4);              // まずは4で十分
    mTargetSigne->SetMaxDeltaFromCenter(0.6f);// ガタつき抑制
    mTargetSigne->SetVisible(true);
   
}

PlayerActor::~PlayerActor() = default;

//======================================================================
// UpdateActor（元ロジック）
//  - SearchTarget は毎フレーム（候補は常に更新）
//  - Battle かつターゲットが有る時だけ OrbitMove + FollowCamera
//  - それ以外は強制的に Field に戻す（元仕様）
//======================================================================
void PlayerActor::UpdateActor(float deltaTime)
{
    // 1) 候補更新（センサー結果 → mCandidates）
    SearchTarget(deltaTime);

    // 2) Move/Camera 切替（元ロジックの分岐）
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        // Battle中 & ターゲットあり：ロックオン移動
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
        // ターゲットなし OR Battleじゃない → Fieldへ戻す（元仕様）
        // ここで状態を全部クリアするので、入力側では「選ぶだけ」にしておくと安定する
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
// SearchTarget（元ロジック + 最小追加）
//  1) Hits → mCandidates 作成
//     - 画面Xで「挿入ソート」（元方式）
//  2) ロック中ターゲットが候補に残っているか再探索（元方式）
//  3) ★追加：WorldToScreen の失敗値（NaN / (ほぼ0,0)）を除外
//  4) ★追加：見失い猶予 + 距離解除（RPG寄り）
//======================================================================
void PlayerActor::SearchTarget(float deltaTime)
{
    const auto hits = mSensor->GetHits();
    mCandidates.clear();

    //========================================================
    // 1) 候補作成（画面Xで挿入ソート）
    //   - この並び順が L1/R1 の左右移動の基準になる
    //========================================================
    for (const auto& h : hits)
    {
        auto* col = h.collider;
        if (!col)
        {
            continue;
        }
        const Vector3 pos = col->GetCenterPosition();
        const auto scInfo = GetApp()->GetRenderer()->WorldToScreen(pos);

        const float x = scInfo.virtualScreen.x;
        const float y = scInfo.virtualScreen.y;

        // WorldToScreen の失敗値対策：
        //  - NaN/Inf を弾く
        //  - (0,0) 近傍を弾く（失敗時に 0,0 を返す実装のため）
        //    これを入れないと候補に「ゴミ」が混入し、
        //    毎フレームの並びが不安定になってロック/選択が暴れる
        if (!std::isfinite(x) || !std::isfinite(y))
        {
            continue;
        }
        if (std::fabs(x) < 1e-4f && std::fabs(y) < 1e-4f)
        {
            continue;
        }

        TargetInfo info;
        info.collider  = col;
        info.screenPos = scInfo.virtualScreen;

        // X昇順で挿入（同値は後ろに積まれる：元ロジックの素朴さを維持）
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

    //========================================================
    // 2) ロック中ターゲットが候補に残っているか（元ロジック）
    //   - 残っていれば、現在のロック位置に index を復元する
    //========================================================
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

    //========================================================
    // 3) ロックが無ければ、見失いタイマはクリア（追加）
    //========================================================
    if (!mTargetCollider)
    {
        mLockLostTime = 0.0f;
        return;
    }

    //========================================================
    // 4) RPG寄り：解除条件（追加）
    //   - 攻撃中（mMovable=false）は見失い猶予を進めない
    //   - 距離が遠すぎる場合は即解除
    //========================================================
    const bool inAttackLock = !mMovable;

    const Vector3 d = (mTargetCollider->GetOwner()->GetPosition() - GetPosition());
    const float distSq  = d.x*d.x + d.y*d.y + d.z*d.z;
    const float breakSq = mLockBreakDist * mLockBreakDist;
    const bool  tooFar  = (distSq > breakSq);

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

    // 遠すぎ OR（攻撃中でなく）猶予超え → ロック解除
    if (tooFar || (!inAttackLock && mLockLostTime > mLockLostGraceSec))
    {
        EnterFieldMode();
        mLockLostTime = 0.0f;
    }
}

//======================================================================
// EnterBattleMode（元ロジック）
//  - Field中に「選択中 index が有効」ならターゲット確定
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
//  - ロック解除だけ行う
//  - Move/Camera/状態の完全リセットは UpdateActor の else 側が担当（元仕様）
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
//  - 入力は「移動」「ターゲット選択」「攻撃」「解除」
//  - モード切替の副作用（Move/Camera）は UpdateActor に任せる
//======================================================================
void PlayerActor::ActorInput(const toy::InputState& state)
{
    // 1) 移動入力（モードで切替）
    if (mPlayMode == PlayMode::Field)
    {
        FieldMove(state);
    }
    else
    {
        BattleMove(state);
    }

    // 2) ターゲット選択（L1/R1）
    SelectTarget(state);

    // 3) 攻撃入力（L2押下中）
    if (state.IsButtonDown(toy::GameButton::L2))
    {
        InputAttack(state);
    }

    // 4) Bでロック解除（L2と競合しないようガード）
    if (state.IsButtonPressed(toy::GameButton::B) && !state.IsButtonDown(toy::GameButton::L2))
    {
        EnterFieldMode();
    }
}

//======================================================================
// SelectTarget（元ロジック）
//  - L1/R1 で mSelectedTarget を移動
//  - 旧ロックを Candidate に戻し、新ロックを Locked にする
//  - Battleへ遷移（副作用の切替は UpdateActor に任せる）
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
        // 初回は中央から開始（元仕様）
        if (mSelectedTarget == NO_TARGET)
        {
            mSelectedTarget = (int)mCandidates.size() / 2;
        }

        // 左端で止まる（ラップしない：元仕様）
        if (mSelectedTarget > 0)
        {
            --mSelectedTarget;
        }
    }

    // R1：右
    if (state.IsButtonPressed(toy::GameButton::R1))
    {
        // 初回は中央から開始（元仕様）
        if (mSelectedTarget == NO_TARGET)
        {
            mSelectedTarget = (int)mCandidates.size() / 2;
        }

        // 右端で止まる（ラップしない：元仕様）
        if (mSelectedTarget < (int)mCandidates.size() - 1)
        {
            ++mSelectedTarget;
        }
    }

    // 押されていないフレームは何もしない
    if (mSelectedTarget == NO_TARGET)
    {
        return;
    }

    // 旧ターゲットを Candidate に戻す
    if (mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::Candidate);
    }

    // 新ターゲットを Locked にする
    mTargetCollider = mCandidates[mSelectedTarget].collider;
    if (mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::Locked);
    }

    // Battleへ（Move/Cameraの切替は UpdateActor が担当）
    mPlayMode     = PlayMode::Battle;
    mLockLostTime = 0.0f;
}

//======================================================================
// InputAttack（元ロジック）
//  - Battle中のみ有効
//  - 攻撃が発生したら mMovable=false にして移動ロック
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
        // 攻撃中は移動不可（解除は ApplyGroundMoveAndAnim 側の「再び動ける条件」で復帰）
        mMovable = false;
    }
}

//----------------------------------------
// フィールド移動：通常移動モーション
//----------------------------------------
void PlayerActor::FieldMove(const toy::InputState& state)
{
    ApplyGroundMoveAndAnim(state, H_Run);
}

//----------------------------------------
// バトル移動：ロックオン移動モーション
//----------------------------------------
void PlayerActor::BattleMove(const toy::InputState& state)
{
    ApplyGroundMoveAndAnim(state, H_WalkSS);
}

//======================================================================
// ApplyGroundMoveAndAnim（元ロジック）
//  - 移動入力 → MoveComponent が速度を持つ
//  - 速度/ジャンプ/攻撃中ロックに応じてアニメ/足音を制御
//======================================================================
void PlayerActor::ApplyGroundMoveAndAnim(const toy::InputState& state,
                                         PlayerMotion moveMotion)
{
    auto* animPlayer = mMeshComp->GetAnimPlayer();
    animPlayer->SetPlayRate(1.5f);

    toy::MoveComponent* move = GetActiveMove();

    if (mMovable)
    {
        // 攻撃入力が立った瞬間はここでロックに落とす（元挙動）
        if (mInputAttack)
        {
            mMovable = false;
        }
        else
        {
            // ジャンプ
            if (state.IsButtonPressed(toy::GameButton::A))
            {
                mGravComp->Jump();
                animPlayer->PlayOnce(H_Jump, H_Stand);
            }

            // 空中
            if (mGravComp->GetVelocityY() != 0.0f)
            {
                animPlayer->Play(H_Jump);
            }
            // 停止
            else if (move->GetForwardSpeed() == 0.0f &&
                     move->GetRightSpeed()   == 0.0f &&
                     move->GetAngularSpeed() == 0.0f)
            {
                animPlayer->Play(H_Stand);
                mSound->Stop();
            }
            // 移動中
            else
            {
                animPlayer->Play(moveMotion);
                if (!mSound->IsPlaying())
                {
                    mSound->Play();
                }
            }
        }

        // 空中では足音停止
        if (mGravComp->GetVelocityY() != 0.0f)
        {
            mSound->Stop();
        }
    }
    else
    {
        // 攻撃中ロック解除：ループor完了で復帰（元ロジック）
        if (animPlayer->IsLooping() || animPlayer->IsFinished())
        {
            mMovable     = true;
            mInputAttack = false;
        }
    }

    // MoveComponent 側にも最終的な可動状態を反映
    move->SetIsMovable(mMovable);
}
