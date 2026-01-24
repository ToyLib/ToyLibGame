//=============================================================================
// PhysWorld_Ground.cpp
//  地面問い合わせ（Collider床 + Terrain）
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace toy {

//=============================================================
// 内部ヘルパー：OBB軸の正規化＋上向き法線統一
//=============================================================
static inline void NormalizeAxes_Up(const OBB& obb, Vector3& ax, Vector3& ay, Vector3& az)
{
    ax = obb.axisX;
    ay = obb.axisY;
    az = obb.axisZ;

    if (ax.LengthSq() > Math::NearZeroEpsilon) ax.Normalize(); else ax = Vector3::UnitX;
    if (ay.LengthSq() > Math::NearZeroEpsilon) ay.Normalize(); else ay = Vector3::UnitY;
    if (az.LengthSq() > Math::NearZeroEpsilon) az.Normalize(); else az = Vector3::UnitZ;

    // 上向きに統一
    if (Vector3::Dot(ay, Vector3::UnitY) < 0.0f)
    {
        ay *= -1.0f;
    }
}

//=============================================================
// 内部ヘルパー：Collider床の「OBB上面」を XZ でサンプルして GroundHit を作る
//  - 成功したら true
//  - startY/endY の区間内だけを許可（RayDown / SweepDown で共通）
//=============================================================
bool PhysWorld::TryGetColliderTopHitAtXZ(const ColliderComponent* c,
                                        float x,
                                        float z,
                                        float startY,
                                        float endY,
                                        float cosMaxSlope,
                                        GroundHit& outHit) const
{
    outHit = GroundHit{};

    if (!c || !c->GetEnabled())
    {
        return false;
    }

    if (!c->HasFlag(C_GROUND))
    {
        return false;
    }

    const auto bv = c->GetBoundingVolume();
    if (!bv) return false;

    const auto obbSP = bv->GetOBB();
    if (!obbSP) return false;

    const OBB& obb = *obbSP;

    Vector3 ax, ay, az;
    NormalizeAxes_Up(obb, ax, ay, az);

    // 傾斜がキツい面は床扱いしない
    if (ay.y < cosMaxSlope)
    {
        return false;
    }

    // 上面中心（上向き ay で）
    const Vector3 topC = obb.pos + ay * obb.radius.y;

    // (x,z) を上面座標( ax, az )へ
    const Vector3 p(x, topC.y, z);
    const Vector3 d = p - topC;

    const float u = Vector3::Dot(d, ax);
    const float v = Vector3::Dot(d, az);

    const float eps = 0.01f;
    if (fabsf(u) > obb.radius.x + eps || fabsf(v) > obb.radius.z + eps)
    {
        return false;
    }

    // 上面平面上の y
    const float y = topC.y + ax.y * u + az.y * v;

    // 区間外
    if (y > startY || y < endY)
    {
        return false;
    }

    outHit.hit      = true;
    outHit.y        = y;
    outHit.pos      = Vector3(x, y, z);
    outHit.normal   = ay;
    outHit.source   = GroundSource::Collider;
    outHit.collider = c;

    // distance は呼び出し側の定義に合わせてセット（RayDownのとき使う）
    outHit.distance = std::max(0.0f, startY - y);

    return true;
}

//=============================================================
// 内部ヘルパー：Terrain も含めて「最良（best）」を更新
//=============================================================
static inline void UpdateBestByHigherY(const GroundHit& candidate, bool& found, float& bestY, GroundHit& bestHit)
{
    if (!candidate.hit) return;
    if (!found || candidate.y > bestY)
    {
        found  = true;
        bestY  = candidate.y;
        bestHit = candidate;
    }
}

//=============================================================================
// Actor 基準で最も近い地面の Y
//=============================================================================
bool PhysWorld::GetNearestGroundY(const Actor* a, float& outY) const
{
    outY = -FLT_MAX;
    if (!a) return false;

    GroundHit hit;
    if (!GetNearestGroundHit(a, hit)) return false;

    outY = hit.y;
    return true;
}

//=============================================================================
// Actor 基準で最も近い地面（Collider + Terrain）
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

    // フォールバック：足が無い Actor など
    return false;
}

//=============================================================================
// 任意XZに対して「最も高い地面」を取得（Collider + Terrain）
//  ※ここは旧AABB.max.yの名残を消して、OBB上面サンプルに統一
//=============================================================================
bool PhysWorld::GetNearestGroundHitAtXZ(const Vector3& pos, GroundHit& outHit) const
{
    outHit = GroundHit{};

    bool  found = false;
    float bestY = -FLT_MAX;
    GroundHit bestHit;

    const float cosMaxSlope = cosf(Math::ToRadians(mMaxGroundSlopeDeg));

    // Collider床：上面サンプル
    for (const ColliderComponent* c : mColliders)
    {
        GroundHit h;
        // ここは上下区間の制約が無い用途なので広めに取る
        const float startY = +FLT_MAX;
        const float endY   = -FLT_MAX;

        if (TryGetColliderTopHitAtXZ(c, pos.x, pos.z, startY, endY, cosMaxSlope, h))
        {
            UpdateBestByHigherY(h, found, bestY, bestHit);
        }
    }

    // Terrain
    GroundHit th;
    if (GetGroundHitAt(pos, th))
    {
        UpdateBestByHigherY(th, found, bestY, bestHit);
    }

    if (!found) return false;

    outHit = bestHit;
    // pos.xz を揃える（Terrain側が別posを持つことがあるなら）
    outHit.pos.x = pos.x;
    outHit.pos.z = pos.z;
    return true;
}

//=============================================================================
// 下向きレイ（垂直線分）で「一番近い床」を取る
//  - origin -> origin.y - maxDist
//  - 返り値の best は “dist が最小”
//=============================================================================
bool PhysWorld::GetGroundHitRayDown(const Vector3& origin,
                                   float maxDist,
                                   uint32_t groundMask,
                                   GroundHit& outHit) const
{
    outHit = GroundHit{};
    if (maxDist <= 0.0f) return false;

    const float endY = origin.y - maxDist;

    bool found = false;
    float bestDist = FLT_MAX;

    const float cosMaxSlope = cosf(Math::ToRadians(mMaxGroundSlopeDeg));

    // Collider床
    for (const ColliderComponent* c : mColliders)
    {
        if (!c || !c->GetEnabled()) continue;
        if (!c->HasFlag(groundMask)) continue;

        GroundHit h;
        if (!TryGetColliderTopHitAtXZ(c, origin.x, origin.z, origin.y, endY, cosMaxSlope, h))
        {
            continue;
        }

        const float dist = origin.y - h.y;
        if (dist < 0.0f || dist > bestDist) continue;

        bestDist = dist;
        h.distance = dist;

        outHit = h;
        found = true;
    }

    // Terrain
    GroundHit th;
    if (GetGroundHitAt(origin, th))
    {
        if (th.y <= origin.y && th.y >= endY)
        {
            const float dist = origin.y - th.y;
            if (!found || dist < bestDist)
            {
                th.distance = dist;
                outHit = th;
                found = true;
            }
        }
    }

    return found;
}

//=============================================================================
// 縦スイープ（startY -> endY）で「最も高い床」を取る
//  - 返り値の best は “y が最大”
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
    if (endY > startY) return false;

    bool  found = false;
    float bestY = -FLT_MAX;
    GroundHit bestHit;

    const float cosMaxSlope = cosf(Math::ToRadians(mMaxGroundSlopeDeg));

    // Collider床
    for (const ColliderComponent* c : mColliders)
    {
        if (!c || c == ignore) continue;
        if (!c->GetEnabled()) continue;
        if (!c->HasFlag(groundMask)) continue;

        GroundHit h;
        if (!TryGetColliderTopHitAtXZ(c, x, z, startY, endY, cosMaxSlope, h))
        {
            continue;
        }

        UpdateBestByHigherY(h, found, bestY, bestHit);
    }

    // Terrain
    {
        Vector3 pos(x, startY, z);
        GroundHit th;
        if (GetGroundHitAt(pos, th))
        {
            if (th.y <= startY && th.y >= endY)
            {
                UpdateBestByHigherY(th, found, bestY, bestHit);
            }
        }
    }

    if (!found) return false;
    outHit = bestHit;
    return true;
}

} // namespace toy
