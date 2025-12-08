#pragma once

#include "ToyLib.h"

#ifndef __HEROMOTION
#define __HEROMOTION
enum HeroMotion
{
    H_Run = 11,
    H_Dead = 0,
    H_Guard = 1,
    H_Jump = 4,
    H_Stand = 17,
    H_Walk = 18,
    H_Slash = 13,
    H_Spin = 14,
    H_Stab = 15
    
};
#endif


class HeroActor : public toy::Actor
{
public:
    HeroActor(class toy::Application* a);
    virtual ~HeroActor();
    void UpdateActor(float deltaTime) override;
    void ActorInput(const struct toy::InputState& state) override;
private:
    enum HeroMotion mAnimID;
    class toy::MoveComponent* mMoveComp;
    class toy::SkeletalMeshComponent* mMeshComp;
    class toy::ColliderComponent* mCollComp;
    class toy::CameraComponent* mCameraComp;
    class toy::GravityComponent* mGravComp;
    class toy::SoundComponent* mSound;
    bool mMovable;
};
