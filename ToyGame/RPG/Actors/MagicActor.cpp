#include "MagicActor.h"
#include "ToyLib.h"

MagicActor::MagicActor(toy::Application* a)
    : toy::Actor(a)
    , mCnt(1000)
    , mForward(Vector3::UnitZ)
    , mSpeed(0.1f)
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
    mCnt++;
    if (mCnt < 40)
    {
        
    }
    else if (mCnt == 20)
    {
        mLight->SetEnabled(true);
    }
    else if (mCnt == 40)
    {
        mParticle->Start();
        mLight->SetEnabled(true);
    }
    else if (mCnt < 300)
    {
    }
    else if (mCnt == 300)
    {
        mParticle->Stop();
        mLight->SetEnabled(false);
    }
    Vector3 v = GetPosition() + mForward * mSpeed;
    SetPosition(v);
    mSpeed += 0.0005f;
}

void MagicActor::Spawn(Vector3 pos, Vector3 front)
{
    mLight->SetEnabled(false);
    mSpeed = 0.1f;
    mCnt = 0;
    mParticle->Stop();
    SetPosition(Vector3(pos.x, pos.y+2.0f, pos.z));
    mForward = front;
}

void MagicActor::Destroy()
{
    
}
