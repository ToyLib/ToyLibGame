#pragma once
#include "ToyLib.h"

class BrickActor : public toy::Actor
{
public:
    BrickActor(toy::Application* a);
    void SetCollFlags(unsigned int type) { coll->SetFlags(type); }
private:
    toy::ColliderComponent* coll;
};
