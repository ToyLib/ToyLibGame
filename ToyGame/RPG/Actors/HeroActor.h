#pragma once

#include "ToyKit.h"

//=============================================================================
// アニメーションクリップ番号（Hero/hero_f.fbx に対応）
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
// HeroActor
//  KitPlayerActor を継承したプレイヤーキャラクター。
//
//  KitPlayerActor が担当するもの（HeroActor では書かない）:
//    - Field / Battle モード切換え・カメラ切換え
//    - ロックオン（SetupSensor を呼べば有効化）
//    - 攻撃中の mMovable ロック管理
//
//  HeroActor が担当するもの:
//    - JSON 設定読み込みによるキャラ初期化
//    - Slash / Spin / Stab + MagicActor / HealMagicActor の攻撃処理
//    - Stand / Run / Jump のアニメーション制御
//=============================================================================
class HeroActor : public toy::kit::KitPlayerActor
{
public:
    HeroActor(toy::Application* a);
    ~HeroActor() override = default;

protected:
    // 攻撃入力（L2 押下中に KitPlayerActor から呼ばれる）
    void OnAttackInput(const toy::InputState& state) override;

    // アニメ・足音制御（毎フレーム KitPlayerActor から呼ばれる）
    void UpdatePlayerAnim(float deltaTime) override;

    // ジャンプなどその他入力（毎フレーム KitPlayerActor から呼ばれる）
    void OnPlayerInput(const toy::InputState& state) override;

private:
    class MagicActor*     mMagic = nullptr;
    class HealMagicActor* mHeal  = nullptr;
    class toy::SoundComponent* mSound = nullptr;
};
