#include "MagicActor.h"
#include "ToyLib.h"

MagicActor::MagicActor(toy::Application* a)
    : toy::Actor(a)
    , mLifeTime(1000.0f)
    , mForward(Vector3::UnitZ)
    , mSpeed(6.0f)
{
    mParticle = CreateComponent<toy::GPUParticleComponent>();
    auto tex = GetApp()->GetAssetManager()->GetTexture("fire.png");
    mParticle->SetTexture(tex);
    mParticle->InitFromFile("ToyGame/Settings/MagicParticle.json");
    mParticle->Stop();
    
    mLight = CreateComponent<toy::PointLightComponent>();
    mLight->SetColor(Vector3(1.0f, 0.8f, 0.1f));
    mLight->SetEnabled(false);
    
}

void MagicActor::UpdateActor(float deltaTime)
{
    mLifeTime += deltaTime;
    if (mLifeTime < 0.5f)
    {
        mParticle->Start();
        mLight->SetEnabled(true);
    }
    else if (mLifeTime < 5.0f)
    {
        Vector3 v = GetPosition() + mForward * mSpeed;// * deltaTime;
        SetPosition(v);
    }
    else
    {
        mParticle->Stop();
        mParticle->Reset();
        mLight->SetEnabled(false);
    }
}

void MagicActor::Spawn(Vector3 pos, Vector3 front)
{
    mParticle->Stop();
    mParticle->Reset();
    mLight->SetEnabled(false);
    mSpeed = 0.1f;
    mLifeTime = 0.0f;
    Vector3 p = pos;
    mForward = front;
    p += front * 2;
    SetPosition(Vector3(p.x, p.y+2.0f, p.z));
}

void MagicActor::Destroy()
{
    
}
