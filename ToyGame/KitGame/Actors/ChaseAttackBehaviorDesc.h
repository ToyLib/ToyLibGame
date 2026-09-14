#pragma once

//=============================================================================
// ChaseAttackBehaviorDesc
//  ChaseAttackBehavior の構築情報。ToyKit::ChaseBehaviorDesc と同じ形の
//  フィールドに、attackRange/attackAnim を足しただけ（ゲーム層のDesc）。
//=============================================================================
struct ChaseAttackBehaviorDesc
{
    float loseDistance        = 30.0f; // 見失い判定の基準距離（XZ距離）
    float moveSpeed           = 8.0f;  // 追跡速度
    float loseRangeMultiplier = 1.5f;  // 見失い判定のヒステリシス（loseDistance * この倍率で見失う）

    float attackRange = 4.0f; // この距離まで詰めたら Attack へ移行する
                               // （小さすぎるとコライダー同士の物理的な押し返しで到達できず
                               //   永久にAttackへ移行しない。ChaseBehaviorDescのstopRangeと同程度が目安）

    // 攻撃終了後、再び攻撃できるようになるまでの間隔（秒）。
    // Attack終了後は Chase に戻るため、Chase に入ってからこの秒数が経つまでは
    // 間合い内でも Attack へ再突入しない（Chase の GetTimer() で判定する）。
    float attackCooldownSec = 1.0f;

    int idleAnim   = 0; // 索敵中/待機中に再生するアニメーションクリップ
    int chaseAnim  = 0; // 追跡中に再生するアニメーションクリップ
    int attackAnim = 0; // 攻撃時に再生するアニメーションクリップ

    float animBlendSec = 0.5f; // Idle/Chase切換え時のアニメーションブレンド時間
};
