#pragma once

#include "ToyLib.h"

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

    void FieldMove(const struct toy::InputState& state);
    void BattleMove(const struct toy::InputState& state);

    // フィールド／バトル共通の地上移動＆アニメ処理
    //   moveMotion: 動いているときに再生するモーションID
    void ApplyGroundMoveAndAnim(const struct toy::InputState& state,
                                PlayerMotion moveMotion);

    // 現在アクティブな MoveComponent を取得
    toy::MoveComponent* GetActiveMove() const { return mMoveComp; }

private:
    static constexpr int NO_TARGET = -1;

    //========================================
    // 状態
    //========================================
    PlayerMotion mAnimID;
    PlayMode     mPlayMode;

    bool mMovable;
    bool mInputAttack;

    int  mSelectedTarget;
    toy::ColliderComponent* mTargetCollider;

    std::vector<TargetInfo> mCandidates;

    //========================================
    // コンポーネント類
    //========================================
    class toy::MoveComponent*        mMoveComp;   // 現在アクティブな MoveComponent
    class toy::DirMoveComponent*     mDirMove;
    class toy::FPSMoveComponent*     mFPSMove;
    class toy::OrbitMoveComponent*   mOrbitMove;

    class toy::SkeletalMeshComponent* mMeshComp;
    class toy::ColliderComponent*     mCollComp;
    class toy::CameraComponent*       mCameraComp;
    class toy::GravityComponent*      mGravComp;
    class toy::SoundComponent*        mSound;
    class toy::SensorComponent*       mSensor;

    //class MagicActor*      mMagic;
    //class HealMagicActor*  mHeal;
};
