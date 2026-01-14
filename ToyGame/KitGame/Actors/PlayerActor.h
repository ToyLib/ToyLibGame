#pragma once

#include "ToyLib.h"
#include <vector>

//============================================================
// プレイヤー用アニメーションID
//  ※ Mesh / AnimationClip 側と対応
//============================================================
enum PlayerMotion
{
    H_Dead     = 0,
    H_Guard    = 1,
    H_Hit1     = 2,
    H_Hit2     = 3,
    H_Jump     = 4,
    H_JumpSS   = 5,
    H_Kick     = 6,
    H_KickSS   = 7,
    H_Pump     = 9,
    H_PumpSS   = 10,
    H_Run      = 11,
    H_RunSS    = 12,
    H_Slash    = 13,
    H_Spin     = 14,
    H_Stab     = 15,
    H_Standing = 16,
    H_Stand    = 17,
    H_Walk     = 18,
    H_WalkSS   = 19
};

//============================================================
// プレイモード
//  - Field  : 通常移動
//  - Battle : ロックオン移動
//============================================================
enum class PlayMode
{
    Field,
    Battle
};

//============================================================
// ターゲット候補情報
//  - screenPos : 画面上X座標でソートするために使用
//============================================================
struct TargetInfo
{
    toy::ColliderComponent* collider = nullptr;
    Vector2                 screenPos = Vector2::Zero;
    bool                    selected  = false; // 現状未使用（将来UI用）
    bool                    locked    = false; // 現状未使用（将来UI用）
};

//============================================================
// PlayerActor
//  - フィールド移動 + ロックオン戦闘
//  - ターゲット選択は「元ロジック」を維持
//============================================================
class PlayerActor : public toy::Actor
{
public:
    PlayerActor(class toy::Application* a);
    virtual ~PlayerActor();

    void UpdateActor(float deltaTime) override;
    void ActorInput(const struct toy::InputState& state) override;

private:
    //========================================================
    // 内部処理（元ロジック）
    //========================================================
    void SearchTarget(float deltaTime);                 // センサー → 候補作成
    void SelectTarget(const struct toy::InputState& state); // L1/R1 選択
    void InputAttack(const struct toy::InputState& state);

    void EnterBattleMode(); // Field → Battle
    void EnterFieldMode();  // Battle → Field

    void FieldMove(const struct toy::InputState& state);
    void BattleMove(const struct toy::InputState& state);

    void ApplyGroundMoveAndAnim(const struct toy::InputState& state,
                                PlayerMotion moveMotion);

    toy::MoveComponent* GetActiveMove() const { return mMoveComp; }

private:
    static constexpr int NO_TARGET = -1;

    //========================================================
    // 状態（元ロジック）
    //========================================================
    PlayerMotion mAnimID;
    PlayMode     mPlayMode;

    bool mMovable;   // 移動可能か（攻撃中ロック用）
    bool mInputAttack;

    // ターゲット管理
    int  mSelectedTarget;                 // mCandidates の index
    toy::ColliderComponent* mTargetCollider { nullptr };// 現在ロック中のCollider
    std::vector<TargetInfo> mCandidates;                // 画面Xでソート済み候補

    //========================================================
    // RPG寄り調整：ロック解除をシビアにしない
    //  ※ 元ロジックに「自然に追加」した部分
    //========================================================
    float mLockLostTime;        // 見失い累積時間
    float mLockLostGraceSec;    // 見失い猶予
    float mLockBreakDist;       // 距離による解除

    //========================================================
    // コンポーネント
    //========================================================
    toy::MoveComponent*         mMoveComp { nullptr };
    toy::DirMoveComponent*      mDirMove  { nullptr };
    toy::FPSMoveComponent*      mFPSMove  { nullptr }; // 将来用
    toy::OrbitMoveComponent*    mOrbitMove{ nullptr };

    toy::SkeletalMeshComponent* mMeshComp { nullptr };
    toy::ColliderComponent*     mCollComp { nullptr };

    toy::OrbitCameraComponent*  mOrbitCamera  { nullptr };
    toy::FollowCameraComponent* mFollowCamera { nullptr };

    toy::GravityComponent*      mGravComp { nullptr };
    toy::SoundComponent*        mSound    { nullptr };
    toy::SensorComponent*       mSensor   { nullptr };
};
