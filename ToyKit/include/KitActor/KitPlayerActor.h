#pragma once

#include "KitActor/KitCharacterActor.h"
#include <vector>

namespace toy::kit {

//=============================================================================
// PlayMode
//=============================================================================
enum class PlayMode { Field, Battle };

//=============================================================================
// KitPlayerActor
//  KitCharacterActor を継承したプレイヤー共通基底クラス。
//
//  担当するもの:
//    - Field / Battle モード切換え（DirMove ⇔ OrbitMove）
//    - カメラ切換え（OrbitCamera ⇔ FollowCamera）
//    - ロックオン（SensorComponent → L1/R1 選択）
//    - 攻撃中の mMovable ロック管理
//
//  派生クラスが担当するもの:
//    - SetupMesh / SetupCollider / SetupGravity などのキャラ固有設定
//    - OnAttackInput() でゲーム固有の攻撃処理
//    - UpdatePlayerAnim() でアニメ・足音制御
//    - OnPlayerInput() でジャンプ等その他の入力
//=============================================================================
class KitPlayerActor : public KitCharacterActor
{
public:
    explicit KitPlayerActor(toy::Application* a);

protected:
    //------------------------------------------------------------------
    // セットアップヘルパー（コンストラクタから呼ぶ）
    //------------------------------------------------------------------
    void SetupMove();
    void SetupCamera();
    void SetupSensor(float fovDeg     = 180.0f,
                     float maxDist    = 90.0f,
                     float nearOverride = 30.0f);

    //------------------------------------------------------------------
    // フック（派生クラスで override）
    //------------------------------------------------------------------
    // L2 押下中に呼ばれる。攻撃アニメ再生 + SetMovable(false) を書く。
    virtual void OnAttackInput(const toy::InputState& /*state*/) {}

    // 毎フレーム呼ばれる。アニメ・足音制御を書く。
    virtual void UpdatePlayerAnim(float /*deltaTime*/) {}

    // ジャンプなど L2 以外の追加入力を書く。
    virtual void OnPlayerInput(const toy::InputState& /*state*/) {}

    //------------------------------------------------------------------
    // 状態照会
    //------------------------------------------------------------------
    bool IsMovable()  const { return mMovable; }
    bool IsInBattle() const { return mPlayMode == PlayMode::Battle; }

    toy::ColliderComponent* GetLockedTarget() const { return mTargetCollider; }
    toy::MoveComponent*     GetMoveComp()     const { return mMoveComp; }

    // mGravity は KitCharacterActor の protected メンバーをそのまま使う
    void SetMovable(bool b) { mMovable = b; }

    //------------------------------------------------------------------
    // ロックオン解除パラメータ（必要なら派生クラスで変更）
    //------------------------------------------------------------------
    float mLockBreakDist    = 35.0f;
    float mLockLostGraceSec = 0.7f;

private:
    //------------------------------------------------------------------
    // KitCharacterActor のフックを実装（final）
    //------------------------------------------------------------------
    void UpdateCharacter(float deltaTime) final;
    void ActorInput(const toy::InputState& state) final;

    //------------------------------------------------------------------
    // 内部処理
    //------------------------------------------------------------------
    void SearchTarget(float deltaTime);
    void SelectTarget(const toy::InputState& state);
    void SwitchModeByState();
    void EnterBattleMode();
    void EnterFieldMode();

    //------------------------------------------------------------------
    // コンポーネント
    //------------------------------------------------------------------
    toy::DirMoveComponent*      mDirMove      = nullptr;
    toy::OrbitMoveComponent*    mOrbitMove    = nullptr;
    toy::MoveComponent*         mMoveComp     = nullptr;

    toy::OrbitCameraComponent*  mOrbitCamera  = nullptr;
    toy::FollowCameraComponent* mFollowCamera = nullptr;

    toy::SensorComponent*       mSensor       = nullptr;

    //------------------------------------------------------------------
    // ロックオン状態
    //------------------------------------------------------------------
    static constexpr int NO_TARGET = -1;

    struct TargetInfo {
        toy::ColliderComponent* collider  = nullptr;
        Vector2                 screenPos = Vector2::Zero;
    };

    std::vector<TargetInfo> mCandidates;
    toy::ColliderComponent* mTargetCollider = nullptr;
    int                     mSelectedTarget = NO_TARGET;
    float                   mLockLostTime   = 0.0f;

    //------------------------------------------------------------------
    // 状態
    //------------------------------------------------------------------
    PlayMode mPlayMode = PlayMode::Field;
    bool     mMovable  = true;
};

} // namespace toy::kit
