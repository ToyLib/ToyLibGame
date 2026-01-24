//=============================================================================
// PhysWorld_Ray.cpp
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"

#include <algorithm>
#include <cmath>

namespace toy {

//=============================================================================
// Ray vs Triangle (Möller–Trumbore)
//=============================================================================
bool IntersectRayTriangle(const Ray& ray,
                          const Vector3& v0,
                          const Vector3& v1,
                          const Vector3& v2,
                          float& outT)
{
    const float EPS = 1e-6f;

    const Vector3 e1 = v1 - v0;
    const Vector3 e2 = v2 - v0;

    const Vector3 p = Vector3::Cross(ray.dir, e2);
    const float det = Vector3::Dot(e1, p);

    if (fabsf(det) < EPS)
    {
        return false;
    }

    const float invDet = 1.0f / det;

    const Vector3 t = ray.start - v0;
    const float u = Vector3::Dot(t, p) * invDet;
    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }

    const Vector3 q = Vector3::Cross(t, e1);
    const float v = Vector3::Dot(ray.dir, q) * invDet;
    if (v < 0.0f || (u + v) > 1.0f)
    {
        return false;
    }

    const float tHit = Vector3::Dot(e2, q) * invDet;
    if (tHit < 0.0f)
    {
        return false;
    }

    outT = tHit;
    return true;
}

//=============================================================================
// Ray vs OBB（スラブ法）
//=============================================================================
bool PhysWorld::IntersectRayOBB(const Ray& ray,
                                const OBB* obb,
                                float& outT) const
{
    const float epsilon = 1e-6f;

    const Vector3 p = obb->pos - ray.start;

    float tMin = 0.0f;
    float tMax = Math::Infinity;

    for (int i = 0; i < 3; ++i)
    {
        Vector3 axis = Vector3::Zero;
        float   r    = 0.0f;

        if (i == 0)
        {
            axis = obb->axisX;
            r    = obb->radius.x;
        }
        else if (i == 1)
        {
            axis = obb->axisY;
            r    = obb->radius.y;
        }
        else
        {
            axis = obb->axisZ;
            r    = obb->radius.z;
        }

        const float e = Vector3::Dot(axis, p);
        const float f = Vector3::Dot(ray.dir, axis);

        if (fabsf(f) > epsilon)
        {
            float t1 = (e + r) / f;
            float t2 = (e - r) / f;

            if (t1 > t2)
            {
                std::swap(t1, t2);
            }

            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);

            if (tMin > tMax)
            {
                return false;
            }
        }
        else
        {
            if (-e - r > 0.0f || -e + r < 0.0f)
            {
                return false;
            }
        }
    }

    outT = tMin;
    return true;
}

//=============================================================================
// Ray vs OBB（法線付き）
//=============================================================================
bool PhysWorld::IntersectRayOBB_WithNormal(const Ray& ray,
                                           const OBB* obb,
                                           float& outT,
                                           Vector3& outNormal) const
{
    const float eps = 1e-6f;

    // center - start
    const Vector3 p = obb->pos - ray.start;

    float tMin = 0.0f;
    float tMax = Math::Infinity;

    Vector3 hitNormal = Vector3::UnitY;

    auto TestAxis = [&](const Vector3& axis, float r) -> bool
    {
        const float e = Vector3::Dot(axis, p);
        const float f = Vector3::Dot(ray.dir, axis);

        if (fabsf(f) > eps)
        {
            // ★ここを IntersectRayOBB と同じ式にする
            float t1 = (e + r) / f;
            Vector3 n1 = axis;          // 進入面の法線候補

            float t2 = (e - r) / f;
            Vector3 n2 = axis * -1.0f;

            if (t1 > t2)
            {
                std::swap(t1, t2);
                std::swap(n1, n2);
            }

            // tMin 更新時に「進入面法線」を保持
            if (t1 > tMin)
            {
                tMin = t1;
                hitNormal = n1;
            }

            tMax = std::min(tMax, t2);
            if (tMin > tMax)
            {
                return false;
            }

            return true;
        }
        else
        {
            // レイがスラブに平行で、かつ外側
            if (-e - r > 0.0f || -e + r < 0.0f)
            {
                return false;
            }
            return true;
        }
    };

    if (!TestAxis(obb->axisX, obb->radius.x)) return false;
    if (!TestAxis(obb->axisY, obb->radius.y)) return false;
    if (!TestAxis(obb->axisZ, obb->radius.z)) return false;

    //==========================================================
    // ★重要：レイ開始点が OBB 内なら遮蔽として無視
    //  - target が壁OBBの中にいる等で「即ヒット」扱いになりやすく、
    //    カメラ補正が暴れて “飛ぶ” 原因になる
    //==========================================================
    if (tMin < 0.0f)
    {
        return false;
    }

    outT = tMin;

    if (hitNormal.LengthSq() > Math::NearZeroEpsilon)
    {
        hitNormal.Normalize();
    }
    else
    {
        hitNormal = Vector3::UnitY;
    }

    outNormal = hitNormal;
    return true;
}
//=============================================================================
// Wall ヒット（start → end の線分）
//=============================================================================
bool PhysWorld::RayHitWall(const Vector3& start,
                           const Vector3& end,
                           Vector3& hitPos) const
{
    Vector3 n;
    return RayHitWallEx(start, end, C_WALL, /*ignoreActor*/nullptr,
                        /*cosFloorLike*/0.0f, hitPos, n);
}

bool PhysWorld::RayHitWallEx(const Vector3& start,
                            const Vector3& end,
                            uint32_t wallMask,
                            const Actor* ignoreActor,
                            float cosFloorLike,          // 例: cos(45deg)=0.707
                            Vector3& outHitPos,
                            Vector3& outHitNormal) const
{
    Ray ray;
    ray.start = start;
    ray.dir   = end - start;

    const float rayLen = ray.dir.Length();
    if (rayLen < Math::NearZeroEpsilon)
    {
        return false;
    }
    ray.dir.Normalize();

    float   closestT = rayLen;
    bool    hit      = false;
    Vector3 bestN    = Vector3::UnitY;

    for (auto* col : mColliders)
    {
        if (!col || !col->GetEnabled())
        {
            continue;
        }

        // 対象マスク（通常 C_WALL を渡す）
        if (!col->HasAnyFlag(wallMask))
        {
            continue;
        }

        // ignoreActor（自分等）を除外したい場合
        if (ignoreActor && col->GetOwner() == ignoreActor)
        {
            continue;
        }

        auto obb = col->GetBoundingVolume()->GetOBB();
        if (!obb)
        {
            continue;
        }

        float   t = 0.0f;
        Vector3 n = Vector3::UnitY;

        if (IntersectRayOBB_WithNormal(ray, obb.get(), t, n))
        {
            if (t < 0.0f || t > closestT)
            {
                continue;
            }

            //========================================================
            // “床っぽい面” を壁として扱わないフィルタ
            //  - n.y が大きい = 上向き = 床/坂の可能性が高い
            //  - 例: 45度までを床扱い -> cos(45)=0.707
            //  - ここを高くするほど「床扱い」が厳しくなる
            //========================================================
            if (cosFloorLike > 0.0f)
            {
                // たまに法線が逆になるケースもあるので絶対値で見る
                const float up = std::fabs(n.y);
                if (up >= cosFloorLike)
                {
                    continue; // 床っぽいので無視（＝止めない）
                }
            }

            closestT = t;
            bestN    = n;
            hit      = true;
        }
    }

    if (!hit)
    {
        return false;
    }

    outHitNormal = bestN;
    outHitPos    = ray.start + ray.dir * (closestT - 0.01f);
    return true;
}

//=============================================================================
// 汎用 Raycast（normal 対応）
//=============================================================================
bool PhysWorld::Raycast(const Vector3& origin,
                        const Vector3& dir,
                        float maxDist,
                        uint32_t flagMask,
                        RaycastHit& outHit) const
{
    outHit = RaycastHit{};

    Ray ray(origin, dir);
    if (ray.dir.LengthSq() < Math::NearZeroEpsilon) return false;

    float   closestT = maxDist;
    bool    hitAny   = false;
    Vector3 bestN    = Vector3::UnitY;

    for (auto* col : mColliders)
    {
        if (!col || !col->GetEnabled()) continue;
        if ((col->GetFlags() & flagMask) == 0) continue;

        auto obb = col->GetBoundingVolume()->GetOBB();
        if (!obb) continue;

        float t = 0.0f;
        Vector3 n = Vector3::UnitY;

        if (IntersectRayOBB_WithNormal(ray, obb.get(), t, n))
        {
            if (t >= 0.0f && t <= closestT)
            {
                closestT = t;
                bestN    = n;
                hitAny   = true;

                outHit.hit      = true;
                outHit.actor    = col->GetOwner();
                outHit.distance = t;
                outHit.point    = ray.start + ray.dir * t;
                outHit.normal   = bestN;
            }
        }
    }

    return hitAny;
}

} // namespace toy
