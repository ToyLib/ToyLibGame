#include "KitPrefab/Humanoid.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Render/IRenderer.h"
#include "Render/LightingManager.h"
#include "Camera/CameraManager.h"
#include "Graphics/Mesh/SkeletalMeshComponent.h"
#include "Physics/ColliderComponent.h"
#include "Physics/GravityComponent.h"
#include "Physics/SensorComponent.h"
#include "Movement/DirMoveComponent.h"
#include "Movement/OrbitMoveComponent.h"
#include "Camera/OrbitCameraComponent.h"
#include "Camera/FollowCameraComponent.h"
#include "Audio/SoundComponent.h"

#include <cmath>

namespace toy::kit {

//-----------------------------------------------------------------------------
Humanoid::Humanoid(toy::Application* app, const HumanoidDesc& desc)
    : Prefab(app)
    , mDesc(desc)
{
    SetupMesh(desc);
    SetupCollider(desc);
    SetupGravity(desc);

    if (desc.enableLockOnCombat)
    {
        SetupMove();
        SetupCamera(desc);
        SetupCombatSensor(desc);
    }

    if (desc.enableVision)
    {
        // Prefab 基底の汎用センサー（Creature と同じ SetupSensor/HasSensorHit）。
        // 上のロックオン用センサー（enableLockOnCombat）とは別の SensorComponent。
        SetupSensor(desc.visionFovDeg, desc.visionMaxDist, desc.visionTargetMask, desc.visionRequireLOS);
    }

    SetupFootstep(desc);

    if (!desc.displayName.empty())
    {
        SetupNameBoard(desc.displayName, desc.fontPath, desc.nameYOffset, desc.nameColor);
    }
    SetupTargetSprites(desc.candidateTexture, desc.lockedTexture);
    SetupAmbientSound(desc.ambientSound, desc.ambientSoundVolume);
    SetupSpeechText(desc.speechText, desc.speechFontPath, desc.speechColor);
}

//=============================================================================
// Interface
//=============================================================================
void Humanoid::SetVisible(bool visible)
{
    if (mMesh) mMesh->SetVisible(visible);
}

void Humanoid::SetCollisionEnabled(bool enabled)
{
    if (mCollider) mCollider->SetEnabled(enabled);
}

void Humanoid::Jump()
{
    if (mGravity) mGravity->Jump();
}

bool Humanoid::IsGrounded() const
{
    return mGravity && mGravity->IsGrounded();
}

float Humanoid::GetVerticalVelocity() const
{
    return mGravity ? mGravity->GetVelocityY() : 0.0f;
}

bool Humanoid::IsMoving() const
{
    return mActiveMove &&
           (mActiveMove->GetForwardSpeed() != 0.0f ||
            mActiveMove->GetRightSpeed()   != 0.0f ||
            mActiveMove->GetAngularSpeed() != 0.0f);
}

void Humanoid::PlayAnimation(int clipIndex)
{
    if (mMesh) mMesh->GetAnimPlayer()->Play(clipIndex);
}

void Humanoid::PlayAnimationBlend(int clipIndex, float blendDuration)
{
    if (!mMesh) return;
    auto* anim = mMesh->GetAnimPlayer();
    anim->PlayBlend(anim->GetAnimID(), clipIndex, blendDuration);
}

void Humanoid::PlayAnimationOnce(int clipIndex, int returnClipIndex)
{
    if (mMesh) mMesh->GetAnimPlayer()->PlayOnce(clipIndex, returnClipIndex);
}

void Humanoid::SetAnimPlayRate(float rate)
{
    if (mMesh) mMesh->GetAnimPlayer()->SetPlayRate(rate);
}

void Humanoid::SelectNextTarget() // R1: 右へ
{
    if (mCandidates.empty()) return;

    if (mSelectedTarget == NO_TARGET)
    {
        mSelectedTarget = static_cast<int>(mCandidates.size()) / 2;
    }
    else if (mSelectedTarget < static_cast<int>(mCandidates.size()) - 1)
    {
        ++mSelectedTarget;
    }
    CommitSelectedTarget();
}

void Humanoid::SelectPrevTarget() // L1: 左へ
{
    if (mCandidates.empty()) return;

    if (mSelectedTarget == NO_TARGET)
    {
        mSelectedTarget = static_cast<int>(mCandidates.size()) / 2;
    }
    else if (mSelectedTarget > 0)
    {
        --mSelectedTarget;
    }
    CommitSelectedTarget();
}

void Humanoid::ReleaseTarget()
{
    EnterFieldMode();
}

//=============================================================================
// セットアップ
//=============================================================================
void Humanoid::SetupMesh(const HumanoidDesc& desc)
{
    mMesh = GetActor()->CreateComponent<toy::SkeletalMeshComponent>(desc.meshDrawOrder);

    auto mesh = GetApp()->GetAssetManager()->GetMesh(desc.model, desc.isRightHanded);
    mMesh->SetMesh(mesh);

    if (!desc.cancelRootTranslationBone.empty())
    {
        mesh->SetCancelRootTranslation(true);
        mesh->SetCancelNodeName(desc.cancelRootTranslationBone);
    }

    mMesh->SetToonRender(desc.toonRender);
    mMesh->SetContourFactor(desc.contourFactor);
    mMesh->SetContourColor(desc.contourColor);
    mMesh->SetYawOffset(Math::ToRadians(desc.yawOffsetDeg));
    mMesh->SetLocalScale(desc.scale);
    mMesh->SetLocalPositon(desc.meshOffset);
}

void Humanoid::SetupCollider(const HumanoidDesc& desc)
{
    mCollider = GetActor()->CreateComponent<toy::ColliderComponent>();
    mCollider->GetBoundingVolume()->ComputeFromMeshComponent(mMesh);
    mCollider->GetBoundingVolume()->AdjustBoundingBox(desc.colliderOffset, desc.colliderScale);
    mCollider->SetFlags(desc.colliderFlags);
    mCollider->SetEnabled(true);

    TrackCollider(mCollider);
}

void Humanoid::SetupGravity(const HumanoidDesc& desc)
{
    if (!desc.useGravity) return;

    mGravity = GetActor()->CreateComponent<toy::GravityComponent>();
    mGravity->SetEnableGroundPose(desc.enableGroundPose);
    mGravity->SetGravityAccel(desc.gravityAccel);
    mGravity->SetJumpSpeed(desc.jumpSpeed);

    TrackGravity(mGravity);
}

void Humanoid::SetupMove()
{
    mDirMove   = GetActor()->CreateComponent<toy::DirMoveComponent>();
    mOrbitMove = GetActor()->CreateComponent<toy::OrbitMoveComponent>();

    mDirMove->SetIsMovable(true);
    mOrbitMove->SetIsMovable(false);
    mActiveMove = mDirMove;
}

void Humanoid::SetupCamera(const HumanoidDesc& desc)
{
    mFollowCamera = GetActor()->CreateComponent<toy::FollowCameraComponent>();
    mOrbitCamera  = GetActor()->CreateComponent<toy::OrbitCameraComponent>();

    GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
    mOrbitCamera->SetIsEnabled(true);
    mOrbitCamera->SetFreezeYInAir(desc.freezeCameraYInAir);

    mFollowCamera->SetIsEnabled(false);
    mFollowCamera->SetFreezeYInAir(desc.freezeCameraYInAir);
}

void Humanoid::SetupCombatSensor(const HumanoidDesc& desc)
{
    toy::SensorComponent::Desc sensorDesc;
    sensorDesc.fovRad                 = Math::ToRadians(desc.sensorFovDeg);
    sensorDesc.maxDist                = desc.sensorMaxDist;
    sensorDesc.requireLOS             = false;
    sensorDesc.nearOverrideDist       = desc.sensorNearOverrideDist;
    sensorDesc.nearOverrideRequireLOS = true;

    mSensor = GetActor()->CreateComponent<toy::SensorComponent>(sensorDesc);
}

void Humanoid::SetupFootstep(const HumanoidDesc& desc)
{
    if (desc.footstepSound.empty()) return;

    mFootstepSound = GetActor()->CreateComponent<toy::SoundComponent>();
    mFootstepSound->SetSound(desc.footstepSound);
    mFootstepSound->SetVolume(desc.footstepVolume);
    mFootstepSound->Enable3DSound(true);
}

//=============================================================================
// 毎フレーム処理
//=============================================================================
void Humanoid::OnUpdate(float deltaTime)
{
    if (mDesc.enableLockOnCombat)
    {
        SearchTarget(deltaTime);
        UpdateModeAndCamera();
    }
    UpdateMovableRecovery();
    UpdateFootstepSound();
}

//-----------------------------------------------------------------------------
// SearchTarget
//  Sensor の検出結果を画面X順に並べ、ロック中ターゲットの解除条件を見る。
//-----------------------------------------------------------------------------
void Humanoid::SearchTarget(float deltaTime)
{
    const auto hits = mSensor->GetHits();
    mCandidates.clear();

    for (const auto& h : hits)
    {
        auto* col = h.collider;
        if (!col) continue;

        const Vector3 pos    = col->GetCenterPosition();
        const auto     scInfo = GetApp()->GetRenderer()->WorldToScreen(pos);

        const float x = scInfo.virtualScreen.x;
        const float y = scInfo.virtualScreen.y;

        // WorldToScreen の失敗値対策（NaN/Inf・(0,0)近傍を除外）
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
    for (int i = 0; i < static_cast<int>(mCandidates.size()); ++i)
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

    // 解除条件：攻撃中(!mMovable)は見失い猶予を進めない。距離が遠すぎれば即解除。
    const bool inAttackLock = !mMovable;

    const Vector3 d      = mTargetCollider->GetOwner()->GetPosition() - GetPosition();
    const float   distSq = d.x * d.x + d.y * d.y + d.z * d.z;
    const float   breakSq = mDesc.lockBreakDist * mDesc.lockBreakDist;
    const bool    tooFar  = (distSq > breakSq);

    const bool stillInCandidates = (mSelectedTarget != NO_TARGET);

    if (stillInCandidates)
    {
        mLockLostTime = 0.0f;
    }
    else if (!inAttackLock)
    {
        mLockLostTime += deltaTime;
    }

    if (tooFar || (!inAttackLock && mLockLostTime > mDesc.lockLostGraceSec))
    {
        EnterFieldMode();
        mLockLostTime = 0.0f;
    }
}

void Humanoid::CommitSelectedTarget()
{
    if (mSelectedTarget == NO_TARGET) return;

    if (mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::Candidate);
    }

    mTargetCollider = mCandidates[mSelectedTarget].collider;
    if (mTargetCollider)
    {
        mTargetCollider->SetTargetState(toy::TargetState::Locked);
    }

    mPlayMode     = PlayMode::Battle;
    mLockLostTime = 0.0f;
}

void Humanoid::EnterFieldMode()
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

//-----------------------------------------------------------------------------
// UpdateModeAndCamera
//  Battle中かつターゲットありのときだけロックオン移動＋追従カメラ。
//  それ以外は Field へ戻す（元仕様のまま）。
//  攻撃中ロック(mMovable)は、アクティブな MoveComponent 側にだけ反映する。
//-----------------------------------------------------------------------------
void Humanoid::UpdateModeAndCamera()
{
    if (mPlayMode == PlayMode::Battle && mTargetCollider)
    {
        mDirMove->SetIsMovable(false);

        mOrbitMove->SetCenterActor(mTargetCollider->GetOwner());
        mOrbitMove->SetIsMovable(mMovable);
        mActiveMove = mOrbitMove;

        GetApp()->GetCameraManager()->SetActiveCamera(mFollowCamera);
        mOrbitCamera->SetIsEnabled(false);
        mFollowCamera->SetIsEnabled(true);
    }
    else
    {
        mOrbitMove->SetIsMovable(false);
        mDirMove->SetIsMovable(mMovable);

        mActiveMove     = mDirMove;
        mPlayMode       = PlayMode::Field;
        mTargetCollider = nullptr;
        mSelectedTarget = NO_TARGET;
        mLockLostTime   = 0.0f;

        GetApp()->GetCameraManager()->SetActiveCamera(mOrbitCamera);
        mOrbitCamera->SetIsEnabled(true);
        mFollowCamera->SetIsEnabled(false);
    }
}

void Humanoid::UpdateMovableRecovery()
{
    if (mMovable || !mMesh) return;

    auto* anim = mMesh->GetAnimPlayer();
    if (anim->IsLooping() || anim->IsFinished())
    {
        mMovable = true;
    }
}

void Humanoid::UpdateFootstepSound()
{
    if (!mFootstepSound) return;

    const bool airborne = (GetVerticalVelocity() != 0.0f);

    if (mMovable && IsMoving() && !airborne)
    {
        if (!mFootstepSound->IsPlaying()) mFootstepSound->Play();
    }
    else
    {
        mFootstepSound->Stop();
    }
}

} // namespace toy::kit
