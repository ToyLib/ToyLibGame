//=============================================================================
// PhysWorld_Ground.cpp
//  地面問い合わせ（Collider床 + Terrain）
//  ・最も高い地面の取得
//  ・縦方向レイ / スイープによる床判定
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"

#include <algorithm>

namespace toy {

//=============================================================================
// Actor 基準で最も近い地面の Y を取得
//=============================================================================
bool PhysWorld::GetNearestGroundY(const Actor* a, float& outY) const
{
    outY = -FLT_MAX;

    if (!a)
    {
        return false;
    }

    GroundHit hit;
    if (!GetNearestGroundHit(a, hit))
    {
        return false;
    }

    outY = hit.y;
    return true;
}

//=============================================================================
// Actor 基準で最も近い地面（Collider + Terrain）を取得
//=============================================================================
bool PhysWorld::GetNearestGroundHit(const Actor* a, GroundHit& outHit) const
{
    outHit = GroundHit{};

    if (!a)
    {
        return false;
    }

    // 足用 Collider があればそれを優先的に使う
    ColliderComponent* foot = FindFootCollider(a);

    Vector3 queryPos = a->GetPosition();
    if (foot)
    {
        const Cube aabb = foot->GetBoundingVolume()->GetWorldAABB();
        queryPos.x = (aabb.min.x + aabb.max.x) * 0.5f;
        queryPos.z = (aabb.min.z + aabb.max.z) * 0.5f;
    }

    return GetNearestGroundHitAtXZ(queryPos, outHit);
}

//=============================================================================
// 任意の XZ に対して「最も高い地面」を取得
//  - Collider床（C_GROUND）と Terrain の両方を見る
//=============================================================================
bool PhysWorld::GetNearestGroundHitAtXZ(const Vector3& pos,
                                       GroundHit& outHit) const
{
    outHit = GroundHit{};

    bool  found = false;
    float bestY = -FLT_MAX;

    //--------------------------------------------------------------------------
    // 1) Collider 床（AABB 上面）
    //--------------------------------------------------------------------------
    for (const ColliderComponent* c : mColliders)
    {
        if (!c)
        {
            continue;
        }

        if (!c->GetEnabled())
        {
            continue;
        }

        if (!c->HasFlag(C_GROUND))
        {
            continue;
        }

        const Cube aabb = c->GetBoundingVolume()->GetWorldAABB();

        // XZ 範囲内かどうか
        if (pos.x < aabb.min.x || pos.x > aabb.max.x ||
            pos.z < aabb.min.z || pos.z > aabb.max.z)
        {
            continue;
        }

        const float yTop = aabb.max.y;
        if (!found || yTop > bestY)
        {
            found        = true;
            bestY        = yTop;
            outHit.hit   = true;
            outHit.y     = yTop;
            outHit.pos   = Vector3(pos.x, yTop, pos.z);
            outHit.normal   = Vector3::UnitY;
            outHit.source   = GroundSource::Collider;
            outHit.collider = c;
        }
    }

    //--------------------------------------------------------------------------
    // 2) Terrain
    //--------------------------------------------------------------------------
    GroundHit terrainHit;
    if (GetGroundHitAt(pos, terrainHit))
    {
        if (!found || terrainHit.y > bestY)
        {
            outHit = terrainHit;
            found  = true;
        }
    }

    return found;
}

//=============================================================================
// 下向きレイで地面を取得
//  - origin から maxDist 下まで
//=============================================================================
bool PhysWorld::GetGroundHitRayDown(const Vector3& origin,
                                   float maxDist,
                                   uint32_t groundMask,
                                   GroundHit& outHit) const
{
    outHit = GroundHit{};

    if (maxDist <= 0.0f)
    {
        return false;
    }

    const float endY = origin.y - maxDist;

    bool  found = false;
    float bestDist = FLT_MAX;

    //--------------------------------------------------------------------------
    // Collider 床
    //--------------------------------------------------------------------------
    for (const ColliderComponent* c : mColliders)
    {
        if (!c)
        {
            continue;
        }

        if (!c->GetEnabled())
        {
            continue;
        }

        if (!c->HasFlag(groundMask))
        {
            continue;
        }

        const Cube aabb = c->GetBoundingVolume()->GetWorldAABB();

        // XZ 範囲チェック
        if (origin.x < aabb.min.x || origin.x > aabb.max.x ||
            origin.z < aabb.min.z || origin.z > aabb.max.z)
        {
            continue;
        }

        const float yTop = aabb.max.y;

        // レイ区間外
        if (yTop > origin.y || yTop < endY)
        {
            continue;
        }

        const float dist = origin.y - yTop;
        if (dist < 0.0f || dist > bestDist)
        {
            continue;
        }

        bestDist = dist;

        outHit.hit      = true;
        outHit.y        = yTop;
        outHit.distance = dist;
        outHit.pos      = Vector3(origin.x, yTop, origin.z);
        outHit.normal   = Vector3::UnitY;
        outHit.source   = GroundSource::Collider;
        outHit.collider = c;

        found = true;
    }

    //--------------------------------------------------------------------------
    // Terrain
    //--------------------------------------------------------------------------
    GroundHit terrainHit;
    if (GetGroundHitAt(origin, terrainHit))
    {
        if (terrainHit.y <= origin.y && terrainHit.y >= endY)
        {
            const float dist = origin.y - terrainHit.y;
            if (!found || dist < bestDist)
            {
                terrainHit.distance = dist;
                outHit = terrainHit;
                found  = true;
            }
        }
    }

    return found;
}

//=============================================================================
// 縦スイープ（startY -> endY）で地面を取得
//=============================================================================
bool PhysWorld::GetGroundHitSweepDown(float startY,
                                     float endY,
                                     float x,
                                     float z,
                                     uint32_t groundMask,
                                     GroundHit& outHit,
                                     const ColliderComponent* ignore) const
{
    outHit = GroundHit{};

    if (endY > startY)
    {
        return false;
    }

    bool found = false;
    float bestY = -FLT_MAX;

    //--------------------------------------------------------------------------
    // Collider 床
    //--------------------------------------------------------------------------
    for (const ColliderComponent* c : mColliders)
    {
        if (!c || c == ignore)
        {
            continue;
        }

        if (!c->GetEnabled())
        {
            continue;
        }

        if (!c->HasFlag(groundMask))
        {
            continue;
        }

        const Cube aabb = c->GetBoundingVolume()->GetWorldAABB();

        if (x < aabb.min.x || x > aabb.max.x ||
            z < aabb.min.z || z > aabb.max.z)
        {
            continue;
        }

        const float yTop = aabb.max.y;
        if (yTop > startY || yTop < endY)
        {
            continue;
        }

        if (!found || yTop > bestY)
        {
            bestY = yTop;

            outHit.hit      = true;
            outHit.y        = yTop;
            outHit.pos      = Vector3(x, yTop, z);
            outHit.normal   = Vector3::UnitY;
            outHit.source   = GroundSource::Collider;
            outHit.collider = c;

            found = true;
        }
    }

    //--------------------------------------------------------------------------
    // Terrain
    //--------------------------------------------------------------------------
    Vector3 pos(x, startY, z);
    GroundHit terrainHit;
    if (GetGroundHitAt(pos, terrainHit))
    {
        if (terrainHit.y <= startY && terrainHit.y >= endY)
        {
            if (!found || terrainHit.y > bestY)
            {
                outHit = terrainHit;
                found  = true;
            }
        }
    }

    return found;
}

} // namespace toy
