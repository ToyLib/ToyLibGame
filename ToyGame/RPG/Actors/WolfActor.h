#pragma once

#include "ToyLib.h"
#include <memory>

class WolfActor : public toy::Actor
{
    enum ActionType
    {
        IDLE,
        WALK,
        RUN
    };
    
public:
    WolfActor(toy::Application* a);
    ~WolfActor();
    
    virtual void UpdateActor(float deltaTime) override;
    
private:
    ActionType mAction;
    toy::SkeletalMeshComponent* meshComp;
    
    unsigned int mCounter;
    
    void ActionIDLE(float deltaTime);
    void ActionWALK(float deltaTime);
    void ActionRUN(float deltaTime);
    
};
