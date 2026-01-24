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
    Ray ray;
    ray.start = start;
    ray.dir   = end - start;

    const float rayLen = ray.dir.Length();
    if (rayLen < Math::NearZeroEpsilon)
    {
        return false;
    }

    ray.dir.Normalize();

    float closestT = rayLen;
    bool hit = false;

    for (auto* col : mColliders)
    {
        if (!col->HasAnyFlag(C_WALL))
        {
            continue;
        }

        auto obb = col->GetBoundingVolume()->GetOBB();
        if (!obb)
        {
            continue;
        }

        float t = 0.0f;
        if (IntersectRayOBB(ray, obb.get(), t))
        {
            if (t < closestT)
            {
                closestT = t;
                hit = true;
            }
        }
    }

    if (!hit)
    {
        return false;
    }

    hitPos = ray.start + ray.dir * (closestT - 0.01f);
    return true;
}

//=============================================================================
// 汎用 Raycast（normal 対応）
//=============================================================================
// 汎用 Raycast（normal 対応）
bool PhysWorld::Raycast(const Vector3& origin,
                        const Vector3& dir,
                        float maxDist,
                        uint32_t flagMask,
                        RaycastHit& outHit) const
{
    outHit = RaycastHit{};

    Ray ray;
    ray.start = origin;
    ray.dir   = dir;

    if (ray.dir.LengthSq() < Math::NearZeroEpsilon)
    {
        return false;
    }

    ray.dir.Normalize();

    const float kTEps = 1e-4f;

    float closestT = maxDist;
    bool hitAny = false;

    for (auto* col : mColliders)
    {
        if (!col->GetEnabled()) continue;
        if ((col->GetFlags() & flagMask) == 0) continue;

        auto obb = col->GetBoundingVolume()->GetOBB();
        if (!obb) continue;

        float   t = 0.0f;
        Vector3 n = Vector3::UnitY;

        if (IntersectRayOBB_WithNormal(ray, obb.get(), t, n))
        {
            if (t > kTEps && t <= closestT)
            {
                closestT        = t;
                hitAny          = true;

                outHit.hit      = true;
                outHit.actor    = col->GetOwner();
                outHit.distance = t;
                outHit.point    = ray.start + ray.dir * t;
                outHit.normal   = n;
            }
        }
    }

    return hitAny;
}

} // namespace toy
