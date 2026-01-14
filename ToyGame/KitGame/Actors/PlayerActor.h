#pragma once

#include "ToyLib.h"
#include <vector>

enum PlayerMotion
{
    H_Dead   = 0,
    H_Guard  = 1,
    H_Hit1   = 2,
    H_Hit2   = 3,
    H_Jump   = 4,
    H_JumpSS = 5,
    H_Kick   = 6,
    H_KickSS = 7,
    H_Pump   = 9,
    H_PumpSS = 10,
    H_Run    = 11,
    H_RunSS  = 12,
    H_Slash  = 13,
    H_Spin   = 14,
    H_Stab   = 15,
    H_Standing = 16,
    H_Stand  = 17,
    H_Walk   = 18,
    H_WalkSS = 19
};

enum class PlayMode
{
    Field,
    Battle
};

struct TargetInfo
{
    toy::ColliderComponent* collider = nullptr;
    Vector2                 screenPos = Vector2::Zero;
    bool                    selected  = false;
    bool                    locked    = false;
};

class PlayerActor : public toy::Actor
{
public:
    PlayerActor(class toy::Application* a);
    virtual ~PlayerActor();

    void UpdateActor(float deltaTime) override;
    void ActorInput(const struct toy::InputState& state) override;

private:
    //========================================
    // 内部ヘルパ（元ロジック）
    //========================================
    void SearchTarget(float deltaTime);
    void SelectTarget(const struct toy::InputState& state);
    void InputAttack(const struct toy::InputState& state);

    void EnterBattleMode();
    void EnterFieldMode();

    void FieldMove(const struct toy::InputState& state);
    void BattleMove(const struct toy::InputState& state);

    void ApplyGroundMoveAndAnim(const struct toy::InputState& state,
                                PlayerMotion moveMotion);

    toy::MoveComponent* GetActiveMove() const { return mMoveComp; }

private:
    static constexpr int NO_TARGET = -1;

    //========================================
    // 状態（元ロジック）
    //========================================
    PlayerMotion mAnimID { H_Stand };
    PlayMode     mPlayMode { PlayMode::Field };

    bool mMovable { true };
    bool mInputAttack { false };

    int  mSelectedTarget { NO_TARGET };                 // ★ index
    toy::ColliderComponent* mTargetCollider { nullptr };// ★ lock pointer
    std::vector<TargetInfo> mCandidates;                // ★ sorted list

    //========================================
    // RPG寄り：ロック解除をシビアにしない（元ロジックに自然に追加）
    //========================================
    float mLockLostTime     { 0.0f };   // 見失い累積
    float mLockLostGraceSec { 0.7f };   // 猶予秒
    float mLockBreakDist    { 35.0f };  // 遠すぎ解除

    //========================================
    // コンポーネント類
    //========================================
    toy::MoveComponent*         mMoveComp { nullptr };
    toy::DirMoveComponent*      mDirMove  { nullptr };
    toy::FPSMoveComponent*      mFPSMove  { nullptr };
    toy::OrbitMoveComponent*    mOrbitMove{ nullptr };

    toy::SkeletalMeshComponent* mMeshComp { nullptr };
    toy::ColliderComponent*     mCollComp { nullptr };

    toy::OrbitCameraComponent*  mOrbitCamera { nullptr };
    toy::FollowCameraComponent* mFollowCamera{ nullptr };

    toy::GravityComponent*      mGravComp { nullptr };
    toy::SoundComponent*        mSound { nullptr };
    toy::SensorComponent*       mSensor { nullptr };
};
