#pragma once

#include "ToyLib.h"

class HealMagicActor : public toy::Actor
{
public:
    HealMagicActor(class toy::Application* a);
    
    void UpdateActor(float deltaTime) override;
    
    void Spawn(Vector3 pos);
    void Destroy();
private:
    class toy::GPUParticleComponent* mParticle;
    class toy::PointLightComponent* mLight;
    unsigned int mCnt;
    Vector3 mPos;
    float mAngle;
    
};
