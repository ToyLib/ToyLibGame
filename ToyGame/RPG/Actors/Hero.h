#pragma once

#include "ToyKit.h"

//=============================================================================
// アニメーションクリップ番号（Hero/hero_m.fbx に対応）
//=============================================================================
#ifndef __HEROMOTION
#define __HEROMOTION
enum HeroMotion
{
    H_Dead    = 0,
    H_Guard   = 1,
    H_Jump    = 5,
    H_Run     = 11,
    H_Stand   = 17,
    H_Walk    = 18,
    H_WalkSS  = 19,   // バトルモード（ロックオン）ストレイフ
    H_Slash   = 13,
    H_Spin    = 14,
    H_Stab    = 15
};
#endif

//=============================================================================
// CastMagicEvent / CastHealEvent
//  Hero が「魔法/回復を発動した」事実を Scene に伝えるための Signal ペイロード。
//  実際に何を出す（Projectile を生成する）かは Scene 側が決める
//  （Hero はエフェクトの生成・寿命管理を持たない）。
//=============================================================================
struct CastMagicEvent
{
    Vector3 position;
    Vector3 forward;
};

struct CastHealEvent
{
    Vector3 position;
};

//=============================================================================
// Hero
//  旧 HeroActor（toy::kit::KitPlayerActor 継承）を、Humanoid Prefab を
//  内包する Game Logic クラスに置き換えた版。
//
//  Humanoid が担当するもの（Hero では書かない）:
//    - Field / Battle モード切換え・カメラ切換え・索敵（enableLockOnCombat）
//    - 攻撃中の移動ロック（SetMovable）とその自動復帰
//
//  Hero が担当するもの:
//    - Slash / Spin / Stab の攻撃トリガー（魔法/回復は Signal で通知するのみ）
//    - Stand / Run / WalkSS / Jump のアニメーション選択
//=============================================================================
class Hero
{
public:
    explicit Hero(toy::Application* app);

    void ProcessInput(const toy::InputState& state);
    void Update(float deltaTime);

    const Vector3&      GetPosition() const { return mBody.GetPosition(); }
    toy::kit::Humanoid& GetBody() { return mBody; }

    // 魔法/回復のエフェクト生成・寿命管理は Scene 側が行う（設計方針 9）
    toy::kit::Signal<CastMagicEvent>& OnCastMagic() { return mOnCastMagic; }
    toy::kit::Signal<CastHealEvent>&  OnCastHeal()  { return mOnCastHeal; }

private:
    void SelectTarget(const toy::InputState& state);
    void OnAttackInput(const toy::InputState& state);
    void OnPlayerInput(const toy::InputState& state);
    void UpdatePlayerAnim(float deltaTime);

    toy::kit::Humanoid mBody;

    toy::kit::Signal<CastMagicEvent> mOnCastMagic;
    toy::kit::Signal<CastHealEvent>  mOnCastHeal;
};
