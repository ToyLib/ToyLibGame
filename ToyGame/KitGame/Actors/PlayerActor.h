#pragma once

#include "ToyLib.h"
#include <functional>

#ifndef __PLAYERMOTION
#define __PLAYERMOTION
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
#endif


class PlayerActor : public toy::Actor
{
public:
    PlayerActor(class toy::Application* a);
    virtual ~PlayerActor();
    void UpdateActor(float deltaTime) override;
    void ActorInput(const struct toy::InputState& state) override;
private:
    void SearchTarget();
    
    std::function<void(const class toy::InputState&)> MoveFunc;
    void FieldMove(const class toy::InputState& state);
private:
    enum PlayerMotion mAnimID;
    class toy::MoveComponent* mMoveComp;
    class toy::SkeletalMeshComponent* mMeshComp;
    class toy::ColliderComponent* mCollComp;
    class toy::CameraComponent* mCameraComp;
    class toy::GravityComponent* mGravComp;
    class toy::SoundComponent* mSound;
    class toy::SensorComponent* mSensor;
    class toy::SpriteComponent* mTarget;
    //class toy::Actor* mTargetActor;
    bool mMovable;
    
    std::vector<toy::Actor*> mCandidateActors;
    toy::Actor* mTargetedActor;

    
    int mSelectedTarget;
    
    //class MagicActor* mMagic;
    //class HealMagicActor* mHeal;
};
