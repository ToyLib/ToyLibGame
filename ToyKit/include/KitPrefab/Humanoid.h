#pragma once

#include "KitPrefab/Prefab.h"
#include "KitPrefab/HumanoidDesc.h"
#include "ToyLib.h"

#include <vector>

namespace toy::kit {

//=============================================================================
// Humanoid
//  人間型キャラクターに使う基本 Prefab（設計方針 4）。
//  Player/NPC で Prefab を分けない（設計方針 4）ため、同じ Humanoid を
//  プレイヤーにも NPC にも使う。
//
//  desc.enableLockOnCombat = true のときだけ、Field/Battle 切換え・
//  追従カメラ・ロックオン（索敵/選択/解除）・足音自動再生が有効になる
//  （プレイヤーが操作する Humanoid 用）。false（既定）の NPC 用途では
//  メッシュ/コライダー/重力・名前表示・ターゲット表示スプライトだけを持ち、
//  索敵・追跡などの行動ロジックは Game Logic 側の AI が Interface
//  （SetPosition/SetRotation 等）越しに行う（設計方針 8）。
//
//  移動そのものの入力（スティック等）は内部の MoveComponent/CameraComponent が
//  自前で toy::InputState を受け取るため、Humanoid 側で中継する必要はない。
//=============================================================================
class Humanoid : public Prefab
{
public:
    Humanoid(toy::Application* app, const HumanoidDesc& desc);
    ~Humanoid() override = default;

    //-------------------------------------------------------------------
    // Interface（設計方針 6）
    //-------------------------------------------------------------------
    void SetVisible(bool visible) override;
    void SetCollisionEnabled(bool enabled) override;

    void  Jump();
    bool  IsGrounded() const;
    float GetVerticalVelocity() const;
    bool  IsMoving() const;

    void SetMovable(bool movable) { mMovable = movable; }
    bool IsMovable() const { return mMovable; }

    void PlayAnimation(int clipIndex) override;
    void PlayAnimationBlend(int clipIndex, float blendDuration) override;
    void PlayAnimationOnce(int clipIndex, int returnClipIndex);
    void SetAnimPlayRate(float rate);

    // ロックオン戦闘（L1/R1/B 相当）
    void SelectNextTarget();
    void SelectPrevTarget();
    void ReleaseTarget();

    bool IsInBattle()      const { return mPlayMode == PlayMode::Battle; }
    bool HasLockedTarget() const { return mTargetCollider != nullptr; }

protected:
    void OnUpdate(float deltaTime) override;

private:
    void SetupMesh(const HumanoidDesc& desc);
    void SetupCollider(const HumanoidDesc& desc);
    void SetupGravity(const HumanoidDesc& desc);
    void SetupMove();
    void SetupCamera(const HumanoidDesc& desc);
    void SetupSensor(const HumanoidDesc& desc);
    void SetupFootstep(const HumanoidDesc& desc);

    void SearchTarget(float deltaTime);
    void CommitSelectedTarget();
    void EnterFieldMode();
    void UpdateModeAndCamera();
    void UpdateMovableRecovery();
    void UpdateFootstepSound();

    HumanoidDesc mDesc;

    toy::SkeletalMeshComponent* mMesh     = nullptr;
    toy::ColliderComponent*     mCollider = nullptr;
    toy::GravityComponent*      mGravity  = nullptr;

    toy::DirMoveComponent*   mDirMove    = nullptr;
    toy::OrbitMoveComponent* mOrbitMove  = nullptr;
    toy::MoveComponent*      mActiveMove = nullptr;

    toy::OrbitCameraComponent*  mOrbitCamera  = nullptr;
    toy::FollowCameraComponent* mFollowCamera = nullptr;

    toy::SensorComponent* mSensor        = nullptr;
    toy::SoundComponent*  mFootstepSound = nullptr;

    static constexpr int NO_TARGET = -1;

    struct TargetInfo
    {
        toy::ColliderComponent* collider  = nullptr;
        Vector2                 screenPos = Vector2::Zero;
    };

    std::vector<TargetInfo> mCandidates;
    toy::ColliderComponent* mTargetCollider = nullptr;
    int                     mSelectedTarget = NO_TARGET;
    float                   mLockLostTime   = 0.0f;

    enum class PlayMode { Field, Battle };
    PlayMode mPlayMode = PlayMode::Field;
    bool     mMovable  = true;
};

} // namespace toy::kit
