//=============================================================================
// PhysWorld_Utility.cpp
//  ・PhysWorld のユーティリティ系実装
//  ・Actor / Collider 検索、天井解決、視界クエリ
//  ・元 PhysWorld.cpp からそのまま分割
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"
#include "Movement/MoveComponent.h"

#include <algorithm>
#include <cmath>

namespace toy {

PhysWorld::PhysWorld()
{}
PhysWorld::~PhysWorld()
{}

//------------------------------------------------------------------------------
// AABBOverlap
//  ・AABB 同士の重なり判定
//  ・eps > 0 でわずかなめり込みを許容
//------------------------------------------------------------------------------
inline bool AABBOverlap(const Cube& a, const Cube& b, float eps = 0.0f)
{
    return
    (a.max.x + eps >= b.min.x) && (a.min.x - eps <= b.max.x) &&
    (a.max.y + eps >= b.min.y) && (a.min.y - eps <= b.max.y) &&
    (a.max.z + eps >= b.min.z) && (a.min.z - eps <= b.max.z);
}

//------------------------------------------------------------------------------
// AABBOverlapXZ
//  ・XZ 平面のみでの重なり判定（床・足元判定用）
//------------------------------------------------------------------------------
inline bool AABBOverlapXZ(const Cube& a, const Cube& b, float eps = 0.0f)
{
    return
    (a.max.x + eps >= b.min.x) && (a.min.x - eps <= b.max.x) &&
    (a.max.z + eps >= b.min.z) && (a.min.z - eps <= b.max.z);
}


//=============================================================================
// FindFootCollider
//  ・Actor が持つ Collider のうち C_FOOT を持つものを返す
//  ・足元判定用（Ground / Gravity 系で使用）
//=============================================================================
ColliderComponent* PhysWorld::FindFootCollider(const Actor* a) const
{
    if (!a)
    {
        return nullptr;
    }

    for (auto* comp : a->GetAllComponents<ColliderComponent>())
    {
        if (!comp)
        {
            continue;
        }
        if (comp->HasAnyFlag(C_FOOT))
        {
            return comp;
        }
    }
    return nullptr;
}

//=============================================================================
// ResolveCeiling
//  ・Actor の Collider（moverFlag）と
//    天井 Collider（ceilingFlag）のめり込みを解消する
//  ・下方向への MTV のみを集計して押し戻す
//=============================================================================
bool PhysWorld::ResolveCeiling(Actor* a,
                               uint32_t moverFlag,
                               uint32_t ceilingFlag,
                               Vector3& outPush) const
{
    outPush = Vector3::Zero;

    if (!a)
    {
        return false;
    }

    const float kBroadEps = 0.02f;
    bool hitAny = false;

    for (auto* c1 : mColliders)
    {
        if (!c1)
        {
            continue;
        }
        if (!c1->GetEnabled())
        {
            continue;
        }
        if (c1->GetOwner() != a)
        {
            continue;
        }
        if (!c1->HasAnyFlag(moverFlag))
        {
            continue;
        }

        const Cube aAabb = c1->GetBoundingVolume()->GetWorldAABB();
        auto aObbSP = c1->GetBoundingVolume()->GetOBB();
        const OBB* aObb = aObbSP.get();
        if (!aObb)
        {
            continue;
        }

        for (auto* c2 : mColliders)
        {
            if (!c2)
            {
                continue;
            }
            if (!c2->GetEnabled())
            {
                continue;
            }
            if (c2->GetOwner() == a)
            {
                continue;
            }
            if (!c2->HasAnyFlag(ceilingFlag))
            {
                continue;
            }

            const Cube bAabb = c2->GetBoundingVolume()->GetWorldAABB();
            if (!AABBOverlap(aAabb, bAabb, kBroadEps))
            {
                continue;
            }

            auto bObbSP = c2->GetBoundingVolume()->GetOBB();
            const OBB* bObb = bObbSP.get();
            if (!bObb)
            {
                continue;
            }

            Contact contact;
            if (!IntersectOBBContact(aObb, bObb, contact))
            {
                continue;
            }

            // 天井：押し戻しが下向きのものだけ採用
            if (contact.mtv.y < -0.0001f)
            {
                outPush += contact.mtv;
                hitAny = true;
            }
        }
    }

    return hitAny;
}

//=============================================================================
// QueryView
//  ・視界（FOV + 距離 + LOS）に入っている Actor を列挙する
//  ・センサー／ロックオン候補などに使用
//=============================================================================
void PhysWorld::QueryView(const ViewQueryDesc& desc,
                          std::vector<ViewQueryHit>& outHits) const
{
    outHits.clear();

    Vector3 fwd = desc.forward;
    if (fwd.LengthSq() < Math::NearZeroEpsilon)
    {
        return;
    }
    fwd.Normalize();

    const float maxDist   = std::max(0.0f, desc.maxDist);
    const float maxDistSq = maxDist * maxDist;

    const float halfFov    = desc.fovRad * 0.5f;
    const float cosHalfFov = std::cos(halfFov);

    const float nearD   = std::max(0.0f, desc.nearOverrideDist);
    const float nearDSq = nearD * nearD;

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

        // Any 判定
        if (desc.flagMask != 0 && (0 == (col->GetFlags() & desc.flagMask)))
        {
            continue;
        }

        Actor* owner = col->GetOwner();
        if (!owner)
        {
            continue;
        }
        if (desc.ignoreActor && owner == desc.ignoreActor)
        {
            continue;
        }

        // ターゲット位置（OBB 中心が取れるならそれ）
        Vector3 targetPos = owner->GetPosition();
        if (auto obb = col->GetBoundingVolume()->GetOBB())
        {
            targetPos = obb->pos;
        }

        Vector3 to = targetPos - desc.origin;
        const float distSq = to.LengthSq();
        if (distSq > maxDistSq)
        {
            continue;
        }

        float dist = 0.0f;
        if (distSq > Math::NearZeroEpsilon)
        {
            dist = std::sqrt(distSq);
            to *= (1.0f / dist);
        }
        else
        {
            to = fwd;
            dist = 0.0f;
        }

        const bool inNear = (nearDSq > 0.0f) && (distSq <= nearDSq);
        const float cosAng = Vector3::Dot(fwd, to);
        const bool inFov = (cosAng >= cosHalfFov);

        if (!inFov && !inNear)
        {
            continue;
        }

        // LOS 判定
        if (desc.requireLOS)
        {
            const bool needLOS = !inNear || desc.nearOverrideRequireLOS;
            if (needLOS && desc.losBlockMask != 0)
            {
                Ray ray;
                ray.start = desc.origin;
                ray.dir   = to;

                bool blocked = false;

                for (auto* blockCol : mColliders)
                {
                    if (!blockCol)
                    {
                        continue;
                    }
                    if (!blockCol->GetEnabled())
                    {
                        continue;
                    }
                    if (0 == (blockCol->GetFlags() & desc.losBlockMask))
                    {
                        continue;
                    }

                    Actor* blockOwner = blockCol->GetOwner();
                    if (!blockOwner)
                    {
                        continue;
                    }
                    if (desc.ignoreActor && blockOwner == desc.ignoreActor)
                    {
                        continue;
                    }
                    if (blockOwner == owner)
                    {
                        continue;
                    }

                    auto obb = blockCol->GetBoundingVolume()->GetOBB();
                    if (!obb)
                    {
                        continue;
                    }

                    float t = 0.0f;
                    if (IntersectRayOBB(ray, obb.get(), t))
                    {
                        if (t > 0.0f && t + 0.02f < dist)
                        {
                            blocked = true;
                            break;
                        }
                    }
                }

                if (blocked)
                {
                    continue;
                }
            }
        }

        ViewQueryHit h;
        h.actor    = owner;
        h.collider = col;
        h.dist     = dist;
        h.cosAngle = cosAng;
        outHits.emplace_back(h);
    }
}

} // namespace toy
