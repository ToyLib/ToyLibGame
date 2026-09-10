#include "HealBurst.h"

#include <cmath>

namespace {

toy::kit::ProjectileDesc MakeHealBurstDesc()
{
    toy::kit::ProjectileDesc desc;
    desc.particleTexture    = "parts.jpg";
    desc.particleConfigPath = "ToyGame/Settings/HealMagicParticle.json";
    desc.lightColor         = Vector3(0.3f, 0.8f, 0.4f);
    desc.lifeTime            = 2.0f; // 旧コードで演出が止まるタイミング
    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
HealBurst::HealBurst(toy::Application* app, const Vector3& position)
    : mBody(app, MakeHealBurstDesc())
    , mOrigin(position.x, position.y + 5.0f, position.z)
{
    mBody.SetPosition(mOrigin);
    mBody.SetVisible(true);
}

//-----------------------------------------------------------------------------
void HealBurst::Update(float deltaTime)
{
    mAngle += 480.0f;
    const float x = mOrigin.x + std::sin(Math::ToRadians(mAngle * deltaTime)) * 2.0f;
    const float z = mOrigin.z + std::cos(Math::ToRadians(mAngle * deltaTime)) * 2.0f;
    mOrigin.y -= 2.5f * deltaTime;

    mBody.SetPosition(Vector3(x, mOrigin.y, z));
}
