#include "KitPrefab/Projectile.h"

#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Graphics/Effect/ParticleComponent.h"
#include "Graphics/Light/PointLightComponent.h"

namespace toy::kit {

//-----------------------------------------------------------------------------
Projectile::Projectile(toy::Application* app, const ProjectileDesc& desc)
    : Prefab(app)
    , mLightColor(desc.lightColor)
    , mLifeTime(desc.lifeTime)
{
    mParticle = GetActor()->CreateComponent<toy::ParticleComponent>();
    if (!desc.particleTexture.empty())
    {
        mParticle->SetTexture(GetApp()->GetAssetManager()->GetTexture(desc.particleTexture));
    }
    if (!desc.particleConfigPath.empty())
    {
        mParticle->InitFromFile(desc.particleConfigPath);
    }
    mParticle->Stop();

    mLight = GetActor()->CreateComponent<toy::PointLightComponent>();
    mLight->SetColor(mLightColor);
    mLight->SetEnabled(false);
}

//=============================================================================
// Interface
//=============================================================================
void Projectile::SetVisible(bool visible)
{
    if (visible)
    {
        mParticle->Start();
        mLight->SetEnabled(true);
        mLight->SetColor(mLightColor);
    }
    else
    {
        mParticle->Stop();
        mParticle->Reset();
        mLight->SetEnabled(false);
    }
}

//=============================================================================
// 毎フレーム処理
//=============================================================================
void Projectile::OnUpdate(float deltaTime)
{
    mElapsed += deltaTime;
}

} // namespace toy::kit
