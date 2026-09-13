//=============================================================================
// PhysWorld_Collision.cpp
//  Collider 管理 / ペア走査 / 衝突コールバック
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Physics/GravityComponent.h"
#include "Engine/Core/Actor.h"
#include "Movement/MoveComponent.h"

#include <algorithm>
#include <iostream>

namespace toy {

//=============================================================================
// 押し出し優先度判定: フレーム間の位置差分で「動いているか」を判定する
//  ・DirMoveComponent/TryMoveWithRayCheck のような速度ベース移動だけでなく、
//    ToyKit の Behavior（FleeBehavior/ChaseBehavior 等）が MovementUtil 経由で
//    Actor::SetPosition を直接叩く移動（MoveComponent を一切経由しない）も
//    ここで検知できるよう、実際の座標の変化量で判定する。
//=============================================================================
void PhysWorld::UpdateMovementTracking()
{
    constexpr float kMoveEpsSq = 1e-6f;

    // 前フレームの判定結果をクリア（mLastActorPositions は履歴として残す）
    mIsMovingThisFrame.clear();

    for (auto* c : mColliders)
    {
        if (!c) continue;

        Actor* actor = c->GetOwner();
        if (!actor) continue;

        // 同じ Actor が複数 Collider を持つ場合は一度だけ判定する
        if (mIsMovingThisFrame.find(actor) != mIsMovingThisFrame.end())
        {
            continue;
        }

        const Vector3 pos = actor->GetPosition();

        bool moving = false;
        auto it = mLastActorPositions.find(actor);
        if (it != mLastActorPositions.end())
        {
            moving = (pos - it->second).LengthSq() > kMoveEpsSq;
        }

        mIsMovingThisFrame[actor] = moving;
        mLastActorPositions[actor] = pos;
    }
}

bool PhysWorld::IsActorMoving(const Actor* actor) const
{
    if (!actor) return false;

    auto it = mIsMovingThisFrame.find(actor);
    return it != mIsMovingThisFrame.end() && it->second;
}

//=============================================================================
// Collider 管理
//=============================================================================
void PhysWorld::AddCollider(ColliderComponent* c)
{
    if (!c)
    {
        return;
    }

    mColliders.emplace_back(c);
}

void PhysWorld::RemoveCollider(ColliderComponent* c)
{
    if (!c)
    {
        return;
    }

    auto it = std::find(mColliders.begin(), mColliders.end(), c);
    if (it == mColliders.end())
    {
        return;
    }

    Actor* owner = c->GetOwner();
    mColliders.erase(it);

    // 他の Collider がまだ同じ Actor を参照していなければ、
    // 移動判定キャッシュ（ダングリングポインタ化を防ぐ）も破棄する
    if (owner)
    {
        const bool stillReferenced = std::any_of(mColliders.begin(), mColliders.end(),
            [owner](const ColliderComponent* other) { return other && other->GetOwner() == owner; });

        if (!stillReferenced)
        {
            mLastActorPositions.erase(owner);
            mIsMovingThisFrame.erase(owner);
        }
    }
}

//=============================================================================
// フラグ検索ユーティリティ
//=============================================================================
void PhysWorld::GetCollidersByAnyFlags(uint32_t mask,
                                      std::vector<ColliderComponent*>& out) const
{
    for (auto* col : mColliders)
    {
        if (!col)
        {
            continue;
        }

        if (!col->GetEnabled())
        {
            continue;
        }

        if ((col->GetFlags() & mask) != 0)
        {
            out.push_back(col);
        }
    }
}

void PhysWorld::GetCollidersByAllFlags(uint32_t mask,
                                      std::vector<ColliderComponent*>& out) const
{
    for (auto* col : mColliders)
    {
        if (!col)
        {
            continue;
        }

        if (!col->GetEnabled())
        {
            continue;
        }

        if ((col->GetFlags() & mask) == mask)
        {
            out.push_back(col);
        }
    }
}

//=============================================================================
// Collider ペア走査 + コールバック + 押し戻し
//  ★ ignoreCollider を追加：この相手との pushback を抑制できる
//  ★ allowY=false の壁ずり用途向けに「接地床の横pushだけ無視」が可能
//=============================================================================
void PhysWorld::CollideAndCallback(uint32_t flagA,
                                   uint32_t flagB,
                                   bool doPushBack,
                                   bool allowY,
                                   bool stopVerticalSpeed)
{
    constexpr float kMinPushSq = 1e-10f;

    for (auto* c1 : mColliders)
    {
        if (!c1 || !c1->GetEnabled()) continue;
        if (!c1->HasAnyFlag(flagA))   continue;

        Actor* ownerA = c1->GetOwner();
        if (!ownerA) continue;

        bool    collided  = false;
        Vector3 bestPush  = Vector3::Zero;
        float   bestLenSq = 0.0f;
        bool    hasPush   = false;

        for (auto* c2 : mColliders)
        {
            if (!c2 || !c2->GetEnabled()) continue;
            if (!c2->HasAnyFlag(flagB))   continue;

            Actor* ownerB = c2->GetOwner();
            if (!ownerB) continue;
            if (ownerA == ownerB) continue;

            if (!(JudgeWithRadius(c1, c2) && JudgeWithOBB(c1, c2)))
            {
                continue;
            }

            // 通知
            c1->Collided(c2);
            c2->Collided(c1);
            collided = true;

            if (!doPushBack) continue;
            if (c1->IsTrigger() || c2->IsTrigger()) continue;

            // 静止側(ownerA)へ移動中の相手(ownerB)がぶつかってきた場合は、
            // 静止側を動かさない。押し出しは ownerB 側のパス
            // (ownerB が flagA として処理される側)に任せる。
            if (!IsActorMoving(ownerA) && IsActorMoving(ownerB))
            {
                continue;
            }

            Vector3 push = ComputePushBackDirection(c1, c2, allowY);
            if (!allowY) push.y = 0.0f;

            const float lenSq = push.LengthSq();
            if (lenSq <= kMinPushSq) continue;

            if (!hasPush || lenSq > bestLenSq)
            {
                bestLenSq = lenSq;
                bestPush  = push;
                hasPush   = true;
            }
        }

        if (collided && hasPush)
        {
            ownerA->SetPosition(ownerA->GetPosition() + bestPush);

            if (stopVerticalSpeed)
            {
                if (auto* move = ownerA->GetComponent<MoveComponent>())
                {
                    move->SetVerticalSpeed(0.0f);
                }
            }
        }
    }
}

//=============================================================================
// PhysWorld 全体テスト（1フレーム分）
//=============================================================================
void PhysWorld::Test()
{
    // 押し出し優先度判定用: 前フレームからの移動量を更新
    UpdateMovementTracking();

    // 前フレームの衝突情報をクリア
    for (auto* c : mColliders)
    {
        if (c)
        {
            c->ClearCollidBuffer();
        }
    }

    // 移動体 vs 壁
    CollideAndCallback(C_PLAYER_TEAM, C_WALL, true, false);
    CollideAndCallback(C_ENEMY_TEAM,  C_WALL, true, false);

    // キャラ同士
    CollideAndCallback(C_PLAYER_TEAM, C_ENEMY_TEAM);

    //============================================================
    // Combat: HITBOX -> HURTBOX
    //============================================================
    auto HasAll = [](const ColliderComponent* c, uint32_t mask)
    {
        if (!c)
        {
            return false;
        }
        if (!c->GetEnabled())
        {
            return false;
        }
        return ((c->GetFlags() & mask) == mask);
    };

    const uint32_t atkMask = C_PLAYER_TEAM | C_HITBOX;
    const uint32_t defMask = C_ENEMY_TEAM  | C_HURTBOX;

    for (auto* c1 : mColliders)
    {
        if (!HasAll(c1, atkMask))
        {
            continue;
        }

        for (auto* c2 : mColliders)
        {
            if (!HasAll(c2, defMask))
            {
                continue;
            }

            if (c1->GetOwner() == c2->GetOwner())
            {
                continue;
            }

            if (JudgeWithRadius(c1, c2) && JudgeWithOBB(c1, c2))
            {
                c1->Collided(c2);
                c2->Collided(c1);
            }
        }
    }
}

} // namespace toy
