#pragma once

#include "Engine/Core/Actor.h"
#include "ToyLib.h"

#include <functional>
#include <string>

class EnemyActor : public toy::Actor
{
public:
    EnemyActor(class toy::Application* a);
    virtual ~EnemyActor();
    
    virtual void UpdateActor(float deltaTime) override;
    using EnemyActionFn = std::function<void(float)>;
private:
    toy::SkeletalMeshComponent* meshComp;
    toy::GroundConformSpriteComponent* mCandidateSigne;
    toy::GroundConformSpriteComponent* mLockOnSigne;
    toy::ColliderComponent* mCollider;
    
    toy::Actor* mNameActor;
    
    float mLifeTime;
    
    
    std::string mEnemyName;

    EnemyActionFn EnemyAction;
    
    void ActionIDLE(float deltaTime);
    void ActionWALK(float deltaTime);
    void ActionRUN(float deltaTime);
};
