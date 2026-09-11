#pragma once

namespace toy::kit {

//=============================================================================
// ChaseBehaviorDesc
//  ChaseBehavior の構築情報（設計方針 5/16）。
//=============================================================================
struct ChaseBehaviorDesc
{
    float detectRange         = 30.0f; // 索敵範囲（XZ距離）
    float moveSpeed           = 8.0f;  // 追跡速度
    float stopRange           = 4.0f;  // 接近停止距離（この距離以内では移動しない）
    float loseRangeMultiplier = 1.5f;  // 見失い判定のヒステリシス（detectRange * この倍率で見失う）

    int idleAnim  = 0; // 索敵中に再生するアニメーションクリップ
    int chaseAnim = 0; // 追跡中に再生するアニメーションクリップ
};

} // namespace toy::kit
