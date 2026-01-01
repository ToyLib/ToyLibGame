#pragma once

#include "Engine/Core/Actor.h"
#include "ToyLib.h"

#include <functional>

class EnemyActor : public toy::Actor
{
public:
    EnemyActor(class toy::Application* a);
    virtual ~EnemyActor();
    
    virtual void UpdateActor(float deltaTime) override;
    using EnemyActionFn = std::function<void(float)>;
private:
    toy::SkeletalMeshComponent* meshComp;
    toy::SpriteComponent* mTarget;
    toy::Actor* mTargetActor;
    toy::ColliderComponent* mCollider;
    
    float mLifeTime;
    

    EnemyActionFn EnemyAction;
    
    void ActionIDLE(float deltaTime);
    void ActionWALK(float deltaTime);
    void ActionRUN(float deltaTime);
};
