#pragma once

#include "KitPrefab/Prefab.h"
#include "KitPrefab/ProjectileDesc.h"
#include "ToyLib.h"

namespace toy::kit {

//=============================================================================
// Projectile
//  弾・魔法・飛翔物・一時エフェクトに使う基本 Prefab（設計方針 4）。
//  パーティクル+ポイントライトの表示/非表示と寿命だけを持つ。
//  飛び方（直進・追尾・螺旋等）や、寿命切れの回収は Game Logic / Scene 側が
//  Interface（SetPosition 等）と IsExpired() 越しに行う（設計方針 8）。
//=============================================================================
class Projectile : public Prefab
{
public:
    Projectile(toy::Application* app, const ProjectileDesc& desc);
    ~Projectile() override = default;

    //-------------------------------------------------------------------
    // Interface（設計方針 6）
    //-------------------------------------------------------------------
    // true: パーティクル再生 + ライト点灯 / false: 停止 + 消灯
    void SetVisible(bool visible) override;

    // コライダーを持たないため何もしない
    void SetCollisionEnabled(bool enabled) override {}

    // クリップアニメーションを持たないため何もしない
    void PlayAnimation(int /*clipIndex*/) override {}
    void PlayAnimationBlend(int /*clipIndex*/, float /*blendDuration*/) override {}

    //-------------------------------------------------------------------
    // 寿命
    //-------------------------------------------------------------------
    float GetElapsed() const { return mElapsed; }
    bool  IsExpired()  const { return mElapsed >= mLifeTime; }

protected:
    void OnUpdate(float deltaTime) override;

private:
    toy::ParticleComponent*   mParticle = nullptr;
    toy::PointLightComponent* mLight    = nullptr;

    Vector3 mLightColor = Vector3::One;
    float   mLifeTime   = 1.0f;
    float   mElapsed    = 0.0f;
};

} // namespace toy::kit
