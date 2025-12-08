#pragma once

#include "ToyLib.h"


class MinionActor : public toy::Actor
{
public:
    MinionActor(class toy::Application* a);
    virtual ~MinionActor();
    void UpdateActor(float deltaTime) override;
private:
    float mCounter;

};

