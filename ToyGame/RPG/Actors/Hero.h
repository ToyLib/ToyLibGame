#pragma once

#include "ToyKit.h"

#include <memory>

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
// HeroControlBehavior
//  Hero の入力処理（IBehavior::OnInput）とアニメーション選択（OnUpdate）。
//
//  Humanoid が担当するもの（ここでは書かない）:
//    - Field / Battle モード切換え・カメラ切換え・索敵（enableLockOnCombat）
//    - 攻撃中の移動ロック（SetMovable）とその自動復帰
//
//  ここが担当するもの:
//    - Slash / Spin / Stab の攻撃トリガー（魔法/回復は Signal で通知するのみ）
//    - Stand / Run / WalkSS / Jump のアニメーション選択
//
//  Humanoid 固有のメソッドを使うため、Prefab& を Humanoid& へ static_cast する
//  （PlayerControlBehavior と同じ考え方）。
//=============================================================================
class HeroControlBehavior : public toy::kit::IBehavior
{
public:
    void OnStart(toy::kit::Prefab& body) override;
    void OnInput(toy::kit::Prefab& body, const toy::InputState& state) override;
    void OnUpdate(toy::kit::Prefab& body, float deltaTime) override;

    // 魔法/回復のエフェクト生成・寿命管理は Scene 側が行う（設計方針 9）
    toy::kit::Signal<CastMagicEvent>& OnCastMagic() { return mOnCastMagic; }
    toy::kit::Signal<CastHealEvent>&  OnCastHeal()  { return mOnCastHeal; }

private:
    void SelectTarget(const toy::InputState& state);
    void OnAttackInput(const toy::InputState& state);
    void OnPlayerInput(const toy::InputState& state);
    void UpdatePlayerAnim();

    toy::kit::Humanoid* mBody = nullptr;

    toy::kit::Signal<CastMagicEvent> mOnCastMagic;
    toy::kit::Signal<CastHealEvent>  mOnCastHeal;
};

//=============================================================================
// Hero
//  Humanoid Prefab + HeroControlBehavior を組み合わせた Agent。
//=============================================================================
class Hero : public toy::kit::Agent<toy::kit::Humanoid>
{
public:
    using Agent::Agent;

    HeroControlBehavior& GetControlBehavior()
    {
        return static_cast<HeroControlBehavior&>(*GetBehavior());
    }
};

std::unique_ptr<Hero> MakeHero(toy::Application* app);
