#pragma once

#include "ToyLib.h"

class IslandActor : public toy::Actor
{
public:
    IslandActor(toy::Application* a);
    virtual void UpdateActor(float deltaTime) override;
private:
    float mLifeTime;
    float mSpeed;
    float mPosY;
    float mPosZ;
};
