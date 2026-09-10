#pragma once

#include "ToyKit.h"

//=============================================================================
// MagicBolt
//  旧 MagicActor（toy::Actor 継承、Hero が使い回す単一インスタンス）を、
//  Projectile Prefab を内包する使い切りの Game Logic クラスに置き換えた版。
//
//  発動から 0.5 秒は演出待ち（非表示）、以降は前方へ直進する。
//  寿命は ProjectileDesc::lifeTime（旧コードで完全に沈黙していたタイミング）
//  で切れ、Scene 側が IsExpired() を見て回収・破棄する。
//=============================================================================
class MagicBolt
{
public:
    MagicBolt(toy::Application* app, const Vector3& position, const Vector3& forward);

    void Update(float deltaTime);
    bool IsExpired() const { return mBody.IsExpired(); }

private:
    static constexpr float kIgniteDelaySec = 0.5f;
    static constexpr float kSpeed          = 0.1f; // 旧コード踏襲（1フレームあたりの移動量）

    toy::kit::Projectile mBody;
    Vector3               mForward;
};
