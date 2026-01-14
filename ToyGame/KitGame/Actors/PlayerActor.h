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
    // 内部ヘルパ
    //========================================
    void SearchTarget();
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
    // 状態
    //========================================
    PlayerMotion mAnimID;
    PlayMode     mPlayMode;

    // ★追加：前フレームのモード（モード遷移を検出するため）
    PlayMode     mPrevPlayMode;

    bool mMovable;
    bool mInputAttack;

    int  mSelectedTarget;
    toy::ColliderComponent* mTargetCollider;

    std::vector<TargetInfo> mCandidates;

    //========================================
    // コンポーネント類
    //========================================
    toy::MoveComponent*         mMoveComp;
    toy::DirMoveComponent*      mDirMove;
    toy::FPSMoveComponent*      mFPSMove;
    toy::OrbitMoveComponent*    mOrbitMove;

    toy::SkeletalMeshComponent* mMeshComp;
    toy::ColliderComponent*     mCollComp;

    toy::OrbitCameraComponent*  mOrbitCamera;
    toy::FollowCameraComponent* mFollowCamera;

    toy::GravityComponent*      mGravComp;
    toy::SoundComponent*        mSound;
    toy::SensorComponent*       mSensor;
};
