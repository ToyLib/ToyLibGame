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
//  desc.enableLockOnCombat = true のときだけ、Free/Locked 切換え・
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

    void TakeDamage(int amount) override;
    bool IsDefeated() const override { return mDesc.maxHp > 0 && mHp <= 0; }

    // HP（HumanoidDesc::maxHp が0のままなら常に0）
    int GetHp()    const { return mHp; }
    int GetMaxHp() const { return mDesc.maxHp; }

    // 近接攻撃用コライダーの有効/無効切り替え（攻撃モーション中だけ true にする想定）
    void SetAttackColliderActive(bool active);

    // ロックオン戦闘（L1/R1/B 相当）
    void SelectNextTarget();
    void SelectPrevTarget();
    void ReleaseTarget();

    bool IsTargetLocked()  const { return mPlayMode == PlayMode::Locked; }
    bool HasLockedTarget() const { return mTargetCollider != nullptr; }

    // Free/Lockedモードが切り替わった事実の通知。これが「カメラ切換え」を
    // 意味することは Humanoid は知らない——どのカメラを有効化するかの対応付けと
    // 実行（CameraManager::SetActiveCamera）は Game Logic 側の責務とする
    // （Player/NPC 共通の Humanoid にカメラの知識を持たせすぎないため）。
    Signal<PlayModeEvent>& OnPlayModeChanged() { return mOnPlayModeChanged; }

    // Game Logic がカメラ切換えを判断する際に使う、Humanoid 自身が所有する
    // カメラ Component へのアクセス（生成できるのは Actor を持つ Prefab 側だけ
    // のため、Component の所有自体は Humanoid に残す）。
    toy::OrbitCameraComponent*  GetOrbitCamera()  const { return mOrbitCamera; }
    toy::FollowCameraComponent* GetFollowCamera() const { return mFollowCamera; }

protected:
    void OnUpdate(float deltaTime) override;

private:
    void SetupMesh(const HumanoidDesc& desc);
    void SetupCollider(const HumanoidDesc& desc);
    void SetupAttackCollider(const HumanoidDesc& desc);
    void SetupGravity(const HumanoidDesc& desc);
    void SetupMove();
    void SetupCamera(const HumanoidDesc& desc);
    void SetupCombatSensor(const HumanoidDesc& desc);
    void SetupFootstep(const HumanoidDesc& desc);

    void HandleCollision(const CollisionEvent& event);

    void SearchTarget(float deltaTime);
    void CommitSelectedTarget();
    void EnterFreeMode();
    void UpdateMode();
    void UpdateMovableRecovery();
    void UpdateFootstepSound();

    HumanoidDesc mDesc;

    toy::SkeletalMeshComponent* mMesh           = nullptr;
    toy::ColliderComponent*     mCollider       = nullptr;
    toy::ColliderComponent*     mAttackCollider = nullptr;
    toy::GravityComponent*      mGravity        = nullptr;

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

    enum class PlayMode { Free, Locked };
    PlayMode mPlayMode = PlayMode::Free;
    bool     mMovable  = true;

    Signal<PlayModeEvent> mOnPlayModeChanged;
    bool                  mWasLocked            = false;
    bool                  mPlayModeEventEmitted = false; // 初回は値に関わらず必ず1回通知する

    int mHp = 0;
};

} // namespace toy::kit
