#include "HealMagicActor.h"
#include "ToyLib.h"

HealMagicActor::HealMagicActor(toy::Application* a)
    : toy::Actor(a)
    , mCnt(1000)
    , mAngle(0.0f)
    , mPos(Vector3::Zero)
{
    mParticle = CreateComponent<toy::GPUParticleComponent>();
    auto tex = GetApp()->GetAssetManager()->GetTexture("parts.jpg");
    mParticle->SetTexture(tex);
    mParticle->InitFromFile("ToyGame/Settings/HealMagicParticle.json");
    mParticle->Stop();
    
    mLight = CreateComponent<toy::PointLightComponent>();
    mLight->SetColor(Vector3(0.3f, 0.8f, 0.4));
    mLight->SetEnabled(false);
    
}

void HealMagicActor::UpdateActor(float deltaTime)
{
    ++mCnt;
    if (mCnt > 120)
    {
        mParticle->Stop();
        mLight->SetEnabled(false);
    }
    else
    {
        mAngle += 12.0f;
        float x = mPos.x + std::sin(Math::ToRadians(mAngle)) * 2.0f;
        float z = mPos.z + std::cos(Math::ToRadians(mAngle)) * 2.0f;
        mPos.y -= 0.05f;
        
        SetPosition(Vector3(x, mPos.y, z));

    }
    

}

void HealMagicActor::Spawn(Vector3 pos)
{
    mLight->SetEnabled(true);
    mParticle->Reset();
    mAngle = 0.1f;
    mPos = pos;
    mPos.y = pos.y + 5.0f;
    mCnt = 0;
    mParticle->Start();
    SetPosition(mPos);
}

void HealMagicActor::Destroy()
{
    
}
