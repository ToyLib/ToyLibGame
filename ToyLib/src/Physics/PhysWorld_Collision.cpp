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
    if (it != mColliders.end())
    {
        mColliders.erase(it);
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
