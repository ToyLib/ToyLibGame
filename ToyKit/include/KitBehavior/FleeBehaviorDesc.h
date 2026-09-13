#pragma once

namespace toy::kit {

//=============================================================================
// FleeBehaviorDesc
//  FleeBehavior の構築情報（設計方針 5/16）。
//=============================================================================
struct FleeBehaviorDesc
{
    // 検知（Idle→Flee）は body 側の視界センサー（HumanoidDesc/CreatureDesc の
    // enableVision 等）で行う。loseDistance は見失い判定（Flee→Idle）にのみ使う
    // （Flee 中はターゲットに背を向けて歩くため、視界センサーでは判定しない。
    //  「見つけるのは視界、諦めるのは距離」という非対称な組み合わせ）。
    float loseDistance = 22.0f; // 見失い判定の基準距離（XZ距離）
    float moveSpeed    = 4.0f;  // 逃走速度

    int idleAnim = 0; // Idle（見失って落ち着いている）に再生するアニメーションクリップ
    int fleeAnim = 0; // 逃走中に再生するアニメーションクリップ

    float animBlendSec = 0.3f; // Idle/Flee切換え時のアニメーションブレンド時間
};

} // namespace toy::kit
