#pragma once

#include "KitActor/KitCharacterActor.h"

namespace toy::kit {

//=============================================================================
// KitNpcActor
//  KitCharacterActor を継承した NPC 共通基底クラス。
//
//  担当するもの:
//    - ターゲット（通常はプレイヤー）への距離計算
//    - XZ 平面での追跡移動
//    - ターゲット方向への向き変換
//
//  派生クラスが担当するもの:
//    - SetupMesh / SetupCollider などのキャラ固有セットアップ
//    - FSM によるステート遷移ロジック
//    - mDetectRange / mMoveSpeed の上書き（必要に応じて）
//=============================================================================

class KitNpcActor : public KitCharacterActor
{
public:
    explicit KitNpcActor(toy::Application* a);

    void        SetTarget(toy::Actor* target) { mTarget = target; }
    toy::Actor* GetTarget()             const { return mTarget; }

protected:
    float mDetectRange = 30.0f;   // 索敵範囲（XZ 距離）
    float mMoveSpeed   = 8.0f;    // 追跡速度

    // ヘルパー ─────────────────────────────────────────────────
    bool  HasTarget()                             const;
    float GetDistanceToTarget()                   const; // XZ 平面距離
    void  MoveTowardTarget(float speed, float dt);      // Y は GravityComponent に委譲
    void  LookAtTarget();                               // Y 軸回転のみ

private:
    toy::Actor* mTarget = nullptr;
};

} // namespace toy::kit
