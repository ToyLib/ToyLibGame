#pragma once

#include "ToyLib.h"

class MagicActor : public toy::Actor
{
public:
    MagicActor(class toy::Application* a);
    
    void UpdateActor(float deltaTime) override;
    
    void Spawn(Vector3 pos, Vector3 front);
    void Destroy();
private:
    class toy::GLParticleComponent* mParticle;
    class toy::PointLightComponent* mLight;
    float mLifeTime;
    Vector3 mForward;
    float mSpeed;
    
};
