#pragma once

#include "ToyKit.h"

//=============================================================================
// HealBurst
//  旧 HealMagicActor（toy::Actor 継承、Hero が使い回す単一インスタンス）を、
//  Projectile Prefab を内包する使い切りの Game Logic クラスに置き換えた版。
//
//  発動位置の周りを螺旋を描きながら落下するエフェクト。
//  寿命は ProjectileDesc::lifeTime（旧コードで演出が止まるタイミング）で切れ、
//  Scene 側が IsExpired() を見て回収・破棄する。
//=============================================================================
class HealBurst
{
public:
    HealBurst(toy::Application* app, const Vector3& position);

    void Update(float deltaTime);
    bool IsExpired() const { return mBody.IsExpired(); }

private:
    toy::kit::Projectile mBody;
    Vector3               mOrigin; // x/z は固定、y だけ毎フレーム落下する
    float                  mAngle = 0.0f;
};
