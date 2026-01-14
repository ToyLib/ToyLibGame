#pragma once
#include "ToyLib.h"

class RPGCharacter : public toy::Actor
{
public:
    RPGCharacter(class toy::Application* a);
    virtual ~RPGCharacter();
    void UpdateActor(float deltaTime) override;
    
    void ActorInput(const struct toy::InputState& state) override;

protected:
    class toy::MoveComponent*           mMoveComp;
    class toy::SkeletalMeshComponent*   mMeshComp;
    class toy::ColliderComponent*       mCollComp;
    class toy::CameraComponent*         mCameraComp;
    class toy::GravityComponent*        mGravComp;
    class toy::SoundComponent*          mSound;
    class toy::SensorComponent*         mSensor;
    class toy::SpriteComponent*         mTarget;
    bool mMovable;
};
