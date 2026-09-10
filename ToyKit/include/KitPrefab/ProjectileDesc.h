#pragma once

#include "Utils/MathUtil.h"

#include <string>

namespace toy::kit {

//=============================================================================
// ProjectileDesc
//  Projectile Prefab の構築情報（設計方針 5/16）。
//  弾・魔法・飛翔物・一時エフェクトなど「パーティクル+ライトで一定時間だけ
//  存在する」ものの見た目と寿命を表す。動き方（直進・螺旋等）はゲーム固有の
//  意味づけなので Game Logic 側が Interface（SetPosition 等）越しに行う。
//=============================================================================
struct ProjectileDesc
{
    // パーティクル
    std::string particleTexture;
    std::string particleConfigPath; // ParticleComponent::InitFromFile に渡すJSON

    // ライト
    Vector3 lightColor = Vector3::One;

    // 寿命（秒）。経過したら IsExpired() が true になる。
    // 実際に破棄するかどうか（Scene側で回収するか等）は Game Logic が判断する。
    float lifeTime = 1.0f;
};

} // namespace toy::kit
