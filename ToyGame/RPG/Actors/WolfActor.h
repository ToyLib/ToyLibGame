#pragma once

#include "ToyLib.h"
#include <memory>

class WolfActor : public toy::Actor
{
public:
    WolfActor(toy::Application* a);
    ~WolfActor();
private:
    toy::SkeletalMeshComponent* meshComp;
};
