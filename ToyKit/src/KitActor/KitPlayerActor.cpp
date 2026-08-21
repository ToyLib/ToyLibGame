#include "KitActor/KitPlayerActor.h"

#include <cmath>

namespace toy::kit {

//-----------------------------------------------------------------------------
KitPlayerActor::KitPlayerActor(toy::Application* a)
    : KitCharacterActor(a)
{
}

//=============================================================================
// セットアップヘルパー
//=============================================================================

void KitPlayerActor::SetupMove()
{
    mDirMove   = CreateComponent<toy::DirMoveComponent>();
    mOrbitMove = CreateComponent<toy::OrbitMoveComponent>();
    mMoveComp  = mDirMove;
    mDirMove->SetIsMovable(true);
    mOrbitMove->SetIsMovable(false);
}

void KitPlayerActor::SetupCamera()
{
    mFollowCamera = CreateComponent<toy::FollowCameraComponent>();
    mOrbitCamera  = CreateComponent<toy::OrbitCameraComponent>();

    GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
    mOrbitCamera->SetIsEnabled(true);
    mOrbitCamera->SetFreezeYInAir(true);
    mFollowCamera->SetIsEnabled(false);
    mFollowCamera->SetFreezeYInAir(true);
}

void KitPlayerActor::SetupSensor(float fovDeg, float maxDist, float nearOverride)
{
    toy::SensorComponent::Desc desc = {
        .fovRad                 = Math::ToRadians(fovDeg),
        .maxDist                = maxDist,
        .requireLOS             = false,
        .nearOverrideDist       = nearOverride,
        .nearOverrideRequireLOS = true
    };
    mSensor = CreateComponent<toy::SensorComponent>(desc);
}

//=============================================================================
// UpdateCharacter（KitCharacterActor::UpdateActor から毎フレーム呼ばれる）
//=============================================================================

void KitPlayerActor::UpdateCharacter(float deltaTime)
{
    SearchTarget(deltaTime);
    SwitchModeByState();
    UpdatePlayerAnim(deltaTime);   // 派生クラスのアニメ制御フック
}

//=============================================================================
// ActorInput（毎フレーム入力処理）
//=============================================================================

void KitPlayerActor::ActorInput(const toy::InputState& state)
{
    // ロックオン候補の L1/R1 選択
    SelectTarget(state);

    // L2 押下中 → 攻撃入力フック
    if (state.IsButtonDown(toy::GameButton::L2))
    {
        OnAttackInput(state);
    }

    // B（L2 なし）→ ロック解除
    if (state.IsButtonPressed(toy::GameButton::B) &&
        !state.IsButtonDown(toy::GameButton::L2))
    {
        EnterFieldMode();
    }

    // その他の入力（ジャンプなど）は派生クラスで
    OnPlayerInput(state);
}

//=============================================================================
// SearchTarget（元 PlayerActor ロジック）
//=============================================================================

void KitPlayerActor::SearchTarget(float deltaTime)
{
    if (!mSensor) return;

    const auto hits = mSensor->GetHits();
    mCandidates.clear();

    // 候補を画面 X 昇順で挿入ソート
    for (const auto& h : hits)
    {
        auto* col = h.collider;
        if (!col) continue;

        const auto scInfo = GetApp()->GetRenderer()->WorldToScreen(col->GetCenterPosition());
        const float x = scInfo.virtualScreen.x;
        const float y = scInfo.virtualScreen.y;

        if (!std::isfinite(x) || !std::isfinite(y)) continue;
        if (std::fabs(x) < 1e-4f && std::fabs(y) < 1e-4f) continue;

        TargetInfo info;
        info.collider  = col;
        info.screenPos = scInfo.virtualScreen;

        auto itr = mCandidates.begin();
        for (; itr != mCandidates.end(); ++itr)
        {
            if (info.screenPos.x < itr->screenPos.x) break;
        }
        mCandidates.insert(itr, info);
    }

    // ロック中ターゲットが候補に残っているか
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

    if (!mTargetCollider)
    {
        mLockLostTime = 0.0f;
        return;
    }

    // 距離チェック
    const Vector3 d = mTargetCollider->GetOwner()->GetPosition() - GetPosition();
    const float distSq  = d.x*d.x + d.y*d.y + d.z*d.z;
    if (distSq > mLockBreakDist * mLockBreakDist)
    {
        EnterFieldMode();
        mLockLostTime = 0.0f;
        return;
    }

    // 見失い猶予（攻撃中は進めない）
    if (mSelectedTarget != NO_TARGET)
    {
        mLockLostTime = 0.0f;
    }
    else
    {
        if (mMovable) mLockLostTime += deltaTime;
        if (mLockLostTime > mLockLostGraceSec)
        {
            EnterFieldMode();
            mLockLostTime = 0.0f;
        }
    }
}

//=============================================================================
// SelectTarget（L1/R1 選択）
//=============================================================================

void KitPlayerActor::SelectTarget(const toy::InputState& state)
{
    if (mCandidates.empty()) return;

    bool moved = false;

    if (state.IsButtonPressed(toy::GameButton::L1))
    {
        if (mSelectedTarget == NO_TARGET)
            mSelectedTarget = (int)mCandidates.size() / 2;
        if (mSelectedTarget > 0)
            --mSelectedTarget;
        moved = true;
    }
    if (state.IsButtonPressed(toy::GameButton::R1))
    {
        if (mSelectedTarget == NO_TARGET)
            mSelectedTarget = (int)mCandidates.size() / 2;
        if (mSelectedTarget < (int)mCandidates.size() - 1)
            ++mSelectedTarget;
        moved = true;
    }

    if (!moved || mSelectedTarget == NO_TARGET) return;

    // 旧ターゲットを Candidate に戻す
    if (mTargetCollider)
        mTargetCollider->SetTargetState(toy::TargetState::Candidate);

    mTargetCollider = mCandidates[mSelectedTarget].collider;
    if (mTargetCollider)
        mTargetCollider->SetTargetState(toy::TargetState::Locked);

    mPlayMode     = PlayMode::Battle;
    mLockLostTime = 0.0f;
}

//=============================================================================
// SwitchModeByState（Move / Camera の切換え）
//=============================================================================

void KitPlayerActor::SwitchModeByState()
{
    if (!mDirMove || !mOrbitMove || !mOrbitCamera || !mFollowCamera) return;

    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        // Battle: OrbitMove + FollowCamera
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
        // Field: DirMove + OrbitCamera
        mOrbitMove->SetIsMovable(false);
        mDirMove->SetIsMovable(true);
        mMoveComp = mDirMove;

        mPlayMode       = PlayMode::Field;
        mTargetCollider = nullptr;
        mSelectedTarget = NO_TARGET;
        mLockLostTime   = 0.0f;

        GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
        mOrbitCamera->SetIsEnabled(true);
        mFollowCamera->SetIsEnabled(false);
    }
}

//=============================================================================
// EnterBattleMode / EnterFieldMode
//=============================================================================

void KitPlayerActor::EnterBattleMode()
{
    if (mPlayMode == PlayMode::Field &&
        mSelectedTarget != NO_TARGET &&
        mSelectedTarget < (int)mCandidates.size())
    {
        mTargetCollider = mCandidates[mSelectedTarget].collider;
        if (mTargetCollider)
            mTargetCollider->SetTargetState(toy::TargetState::Locked);
        mPlayMode = PlayMode::Battle;
    }
}

void KitPlayerActor::EnterFieldMode()
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

} // namespace toy::kit
