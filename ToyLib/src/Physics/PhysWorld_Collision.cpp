//=============================================================================
// PhysWorld_Collision.cpp
//  Collider 管理 / ペア走査 / 衝突コールバック
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Engine/Core/Actor.h"
#include "Movement/MoveComponent.h"

#include <algorithm>

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
//=============================================================================
void PhysWorld::CollideAndCallback(uint32_t flagA,
                                   uint32_t flagB,
                                   bool doPushBack,
                                   bool allowY,
                                   bool stopVerticalSpeed)
{
    // 極小 push は「押していない」とみなすための閾値
    constexpr float kMinPushSq = 1e-10f;

    for (auto* c1 : mColliders)
    {
        if (!c1 || !c1->GetEnabled())
        {
            continue;
        }

        if (!c1->HasAnyFlag(flagA))
        {
            continue;
        }

        bool collided = false;

        //============================================================
        // 「最も強い押し」1つだけを採用する
        //============================================================
        Vector3 bestPush  = Vector3::Zero;
        float   bestLenSq = 0.0f;
        bool    hasPush   = false;

        for (auto* c2 : mColliders)
        {
            if (!c2 || !c2->GetEnabled())
            {
                continue;
            }

            if (!c2->HasAnyFlag(flagB))
            {
                continue;
            }

            if (c1->GetOwner() == c2->GetOwner())
            {
                continue;
            }

            // Broad + Narrow
            if (JudgeWithRadius(c1, c2) && JudgeWithOBB(c1, c2))
            {
                // 衝突通知（Trigger 含む）
                c1->Collided(c2);
                c2->Collided(c1);

                collided = true;

                // 押し戻し対象か？
                if (!doPushBack)
                {
                    continue;
                }

                if (c1->IsTrigger() || c2->IsTrigger())
                {
                    continue;
                }

                Vector3 push = ComputePushBackDirection(c1, c2, allowY);

                // 壁ずり用途：Y 成分を切る
                if (!allowY)
                {
                    push.y = 0.0f;
                }

                const float lenSq = push.LengthSq();

                // 極小 push は無視
                if (lenSq <= kMinPushSq)
                {
                    continue;
                }

                // 最大 push を採用
                if (!hasPush || lenSq > bestLenSq)
                {
                    bestLenSq = lenSq;
                    bestPush  = push;
                    hasPush   = true;
                }
            }
        }

        //============================================================
        // 押し戻し適用
        //============================================================
        if (collided && hasPush)
        {
            Actor* owner = c1->GetOwner();
            owner->SetPosition(owner->GetPosition() + bestPush);

            // 垂直速度停止（床・天井用途）
            if (stopVerticalSpeed)
            {
                if (auto* move = owner->GetComponent<MoveComponent>())
                {
                    move->SetVerticalSpeed(0.0f);
                }
            }
        }
        else if (collided && doPushBack)
        {
            // 衝突したが有効な push が無いケース
            // （ログ用途などで残している想定）
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
