#pragma once

#include "ToyLib.h"
#include <functional>

enum PlayerMotion
{
    H_Run   = 11,
    H_Dead  = 0,
    H_Guard = 1,
    H_Jump  = 5,
    H_Stand = 17,
    H_Walk  = 18,
    H_Slash = 13,
    H_Spin  = 14,
    H_Stab  = 15

};

struct TargetInfo
{
    toy::ColliderComponent* collider = nullptr;
    Vector2 screenPos                = Vector2::Zero;
    bool selected                    = false;
    bool locked                      = false;
};


class PlayerActor : public toy::Actor
{
public:
    PlayerActor(class toy::Application* a);
    virtual ~PlayerActor();
    void UpdateActor(float deltaTime) override;
    void ActorInput(const struct toy::InputState& state) override;
private:
    void SearchTarget();
    void SelectTarget(const struct toy::InputState& state);
    
    std::function<void(const class toy::InputState&)> MoveFunc;
    void FieldMove(const struct toy::InputState& state);
private:
    enum PlayerMotion mAnimID;
    class toy::MoveComponent* mMoveComp;
    class toy::SkeletalMeshComponent* mMeshComp;
    class toy::ColliderComponent* mCollComp;
    class toy::CameraComponent* mCameraComp;
    class toy::GravityComponent* mGravComp;
    class toy::SoundComponent* mSound;
    class toy::SensorComponent* mSensor;
    //class toy::SpriteComponent* mTargetSprite;
    //class toy::Actor* mTargetActor;
    bool mMovable;
    
    std::vector<struct TargetInfo> mCandidates;
    toy::ColliderComponent* mTargetCollider;

    
    int mSelectedTarget;
    
    //class MagicActor* mMagic;
    //class HealMagicActor* mHeal;
};
