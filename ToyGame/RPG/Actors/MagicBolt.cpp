#include "MagicBolt.h"

namespace {

toy::kit::ProjectileDesc MakeMagicBoltDesc()
{
    toy::kit::ProjectileDesc desc;
    desc.particleTexture     = "fire.png";
    desc.particleConfigPath  = "ToyGame/Settings/MagicParticle.json";
    desc.lightColor          = Vector3(1.0f, 0.8f, 0.1f);
    desc.lifeTime             = 5.0f; // 旧コードで移動が止まる（＝事実上の終了）タイミング
    return desc;
}

} // namespace

//-----------------------------------------------------------------------------
MagicBolt::MagicBolt(toy::Application* app, const Vector3& position, const Vector3& forward)
    : mBody(app, MakeMagicBoltDesc())
    , mForward(forward)
{
    Vector3 p = position + forward * 2.0f;
    mBody.SetPosition(Vector3(p.x, p.y + 2.0f, p.z));
}

//-----------------------------------------------------------------------------
void MagicBolt::Update(float /*deltaTime*/)
{
    if (mBody.GetElapsed() < kIgniteDelaySec)
    {
        mBody.SetVisible(true);
    }
    else
    {
        // 旧コード踏襲: deltaTime を乗算しない固定歩幅
        mBody.SetPosition(mBody.GetPosition() + mForward * kSpeed);
    }
}
