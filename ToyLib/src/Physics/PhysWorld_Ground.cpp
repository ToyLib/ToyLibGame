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
    if (!a) return false;

    // まず Footサンプル接地（坂＆角落下対応）
    if (GetFootGroundHit_Sampled(a, C_GROUND, outHit))
    {
        return true;
    }

    // フォールバック：従来のロジック（あれば）
    // 例：足が無い Actor / 古い接地方式など
    // return GetNearestGroundHit_Legacy(a, outHit);

    return false;
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

    bool  found    = false;
    float bestDist = FLT_MAX;

    //--------------------------------------------------------------------------
    // Collider 床（OBB 上面を“傾いた平面”としてサンプル）
    //--------------------------------------------------------------------------
    for (const ColliderComponent* c : mColliders)
    {
        if (!c || !c->GetEnabled())
        {
            continue;
        }

        if (!c->HasFlag(groundMask))
        {
            continue;
        }

        const auto bv = c->GetBoundingVolume();
        if (!bv)
        {
            continue;
        }

        const auto obbSP = bv->GetOBB();
        if (!obbSP)
        {
            continue;
        }

        const OBB& obb = *obbSP;

        // まず axis が正規化されてないケースの保険
        Vector3 ax = obb.axisX;
        Vector3 ay = obb.axisY;
        Vector3 az = obb.axisZ;

        if (ax.LengthSq() > Math::NearZeroEpsilon) ax.Normalize(); else ax = Vector3::UnitX;
        if (ay.LengthSq() > Math::NearZeroEpsilon) ay.Normalize(); else ay = Vector3::UnitY;
        if (az.LengthSq() > Math::NearZeroEpsilon) az.Normalize(); else az = Vector3::UnitZ;

        // 上面中心
        const Vector3 topC = obb.pos + ay * obb.radius.y;

        // origin の xz を、上面平面上の (u,v) に落とす
        // d は topC から origin の水平位置へ向かうベクトル（yは無視してOK）
        const Vector3 p(origin.x, topC.y, origin.z);
        const Vector3 d = p - topC;

        const float u = Vector3::Dot(d, ax);
        const float v = Vector3::Dot(d, az);

        // 上面の矩形投影内か？（OBBの上面範囲チェック）
        if (fabsf(u) > obb.radius.x || fabsf(v) > obb.radius.z)
        {
            continue;
        }

        // 傾いた上面の y を復元
        const float y = topC.y + ax.y * u + az.y * v;

        // レイ区間外
        if (y > origin.y || y < endY)
        {
            continue;
        }

        const float dist = origin.y - y;
        if (dist < 0.0f || dist > bestDist)
        {
            continue;
        }

        bestDist = dist;

        outHit.hit      = true;
        outHit.y        = y;
        outHit.distance = dist;
        outHit.pos      = Vector3(origin.x, y, origin.z);

        // 上面法線（“床の傾き”として使う）
        outHit.normal   = ay;
        outHit.source   = GroundSource::Collider;
        outHit.collider = c;

        found = true;
    }

    //--------------------------------------------------------------------------
    // Terrain（既存のまま）
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

    bool  found = false;
    float bestY = -FLT_MAX;

    //--------------------------------------------------------------------------
    // Collider 床（OBB上面を xz でサンプルして高さを出す）
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

        auto obbPtr = c->GetBoundingVolume()->GetOBB();
        if (!obbPtr)
        {
            continue;
        }

        const OBB& obb = *obbPtr;

        // OBB の "上" 法線候補（ローカルY軸）
        Vector3 n = obb.axisY;
        if (n.LengthSq() <= Math::NearZeroEpsilon)
        {
            continue;
        }
        n.Normalize();

        // 下向き面になってたら反転（常に上向きとして扱う）
        if (Vector3::Dot(n, Vector3::UnitY) < 0.0f)
        {
            n *= -1.0f;
        }

        // ほぼ垂直面は床として扱わない（斜面の上面だけ欲しい）
        // ※ここは調整ポイント。0.2f=約78度, 0.5f=60度, 0.7f=45度
        const float minUp = 0.2f;
        if (n.y < minUp)
        {
            continue;
        }

        // 上面平面上の1点（center + up * radiusY）
        const Vector3 p0 = obb.pos + obb.axisY * obb.radius.y;

        // 垂直線分 (x,*,z) と平面の交点 y を解く
        // dot(n, (x,y,z) - p0) = 0
        // => y = p0.y - (n.x*(x-p0.x) + n.z*(z-p0.z)) / n.y
        const float denom = n.y;
        if (std::fabs(denom) < 1e-6f)
        {
            continue;
        }

        const float y =
            p0.y - (n.x * (x - p0.x) + n.z * (z - p0.z)) / denom;

        // sweep 範囲内だけ
        if (y > startY || y < endY)
        {
            continue;
        }

        // 交点が上面の矩形内かチェック（OBBローカルX/Z半径）
        const Vector3 hitPos(x, y, z);
        const Vector3 d = hitPos - obb.pos;

        const float lx = Vector3::Dot(d, obb.axisX);
        const float lz = Vector3::Dot(d, obb.axisZ);

        const float eps = 0.01f;
        if (std::fabs(lx) > obb.radius.x + eps ||
            std::fabs(lz) > obb.radius.z + eps)
        {
            continue;
        }

        // これが床候補（XZでサンプルした上面の高さ）
        if (!found || y > bestY)
        {
            bestY = y;

            outHit.hit      = true;
            outHit.y        = y;
            outHit.pos      = hitPos;
            outHit.normal   = n;
            outHit.source   = GroundSource::Collider;
            outHit.collider = c;

            found = true;
        }
    }

    //--------------------------------------------------------------------------
    // Terrain（既存どおり）
    //--------------------------------------------------------------------------
    {
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
    }

    return found;
}
} // namespace toy
