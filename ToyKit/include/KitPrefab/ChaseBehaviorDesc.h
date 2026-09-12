#pragma once

namespace toy::kit {

//=============================================================================
// ChaseBehaviorDesc
//  ChaseBehavior の構築情報（設計方針 5/16）。
//=============================================================================
struct ChaseBehaviorDesc
{
    // 索敵（Idle→Chase）自体は body 側の視界センサー（HumanoidDesc/CreatureDesc の
    // enableVision 等）で行う。detectRange はもう「検知距離」ではなく、見失い判定
    // （Chase→Idle）のヒステリシスの基準距離としてのみ使う。
    float detectRange         = 30.0f; // 見失い判定の基準距離（XZ距離）
    float moveSpeed           = 8.0f;  // 追跡速度
    float stopRange           = 4.0f;  // 接近停止距離。この距離以内に近づくと移動をやめ、Idleへ戻る
                                        // （索敵の再検知はSensorのみで行われるため、視界に居続けても
                                        // 近すぎる間はChaseへ戻らない＝ここでの停止がそのままヒステリシスになる）
    float loseRangeMultiplier = 1.5f;  // 見失い判定のヒステリシス（detectRange * この倍率で見失う）

    int idleAnim  = 0; // Idle（索敵中/接近しすぎて待機中）に再生するアニメーションクリップ
    int chaseAnim = 0; // 追跡中に再生するアニメーションクリップ

    float animBlendSec = 0.5f; // Idle/Chase切換え時のアニメーションブレンド時間
};

} // namespace toy::kit
