//=============================================================================
// PhysWorld_MTV.cpp
//  OBB / MTV / 押し戻し計算・接触情報
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"

#include <cmath>

namespace toy {

//=============================================================================
// OBB 判定（SAT 基本）
//=============================================================================
bool PhysWorld::CompareLengthOBB(const OBB* cA,
                                 const OBB* cB,
                                 const Vector3& vSep,
                                 const Vector3& vDistance) const
{
    const float kAxisEps = 1e-6f;
    const float lenSq = vSep.LengthSq();

    // 分離軸がほぼゼロ → 判定不要（重なっている扱い）
    if (lenSq < kAxisEps)
    {
        return true;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    const Vector3 sepN = vSep * invLen;

    // 軸方向距離
    const float length = fabsf(Vector3::Dot(sepN, vDistance));

    // 各 OBB の射影半径
    const float lenA =
        fabsf(Vector3::Dot(cA->axisX, sepN) * cA->radius.x) +
        fabsf(Vector3::Dot(cA->axisY, sepN) * cA->radius.y) +
        fabsf(Vector3::Dot(cA->axisZ, sepN) * cA->radius.z);

    const float lenB =
        fabsf(Vector3::Dot(cB->axisX, sepN) * cB->radius.x) +
        fabsf(Vector3::Dot(cB->axisY, sepN) * cB->radius.y) +
        fabsf(Vector3::Dot(cB->axisZ, sepN) * cB->radius.z);

    return (length <= (lenA + lenB));
}

bool PhysWorld::JudgeWithOBB(const ColliderComponent* col1,
                             const ColliderComponent* col2) const
{
    auto obb1 = col1->GetBoundingVolume()->GetOBB();
    auto obb2 = col2->GetBoundingVolume()->GetOBB();
    return IsCollideBoxOBB(obb1.get(), obb2.get());
}

bool PhysWorld::IsCollideBoxOBB(const OBB* cA, const OBB* cB) const
{
    const Vector3 vDistance = cB->pos - cA->pos;

    // 15 軸 SAT
    if (!CompareLengthOBB(cA, cB, cA->axisX, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cA->axisY, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cA->axisZ, vDistance)) { return false; }

    if (!CompareLengthOBB(cA, cB, cB->axisX, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cB->axisY, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cB->axisZ, vDistance)) { return false; }

    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisX, cB->axisX), vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisX, cB->axisY), vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisX, cB->axisZ), vDistance)) { return false; }

    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisY, cB->axisX), vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisY, cB->axisY), vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisY, cB->axisZ), vDistance)) { return false; }

    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisZ, cB->axisX), vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisZ, cB->axisY), vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, Vector3::Cross(cA->axisZ, cB->axisZ), vDistance)) { return false; }

    return true;
}

//=============================================================================
// 半径（Sphere）判定（Broad Phase）
//=============================================================================
bool PhysWorld::JudgeWithRadius(const ColliderComponent* col1,
                                const ColliderComponent* col2) const
{
    const Vector3 d = col1->GetPosition() - col2->GetPosition();
    const float len = d.Length();

    const float r =
        col1->GetBoundingVolume()->GetRadius() +
        col2->GetBoundingVolume()->GetRadius();

    return (len < r);
}

//=============================================================================
// MTV（最小押し戻し量）算出
//=============================================================================
bool PhysWorld::CompareLengthOBB_MTV(const OBB* cA,
                                     const OBB* cB,
                                     const Vector3& vSep,
                                     const Vector3& vDistance,
                                     MTVResult& mtv) const
{
    const float kAxisEps = 1e-6f;
    const float lenSq = vSep.LengthSq();

    if (lenSq < kAxisEps)
    {
        return true;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    const Vector3 sepN = vSep * invLen;

    const float length = fabsf(Vector3::Dot(sepN, vDistance));

    const float lenA =
        fabsf(Vector3::Dot(cA->axisX, sepN) * cA->radius.x) +
        fabsf(Vector3::Dot(cA->axisY, sepN) * cA->radius.y) +
        fabsf(Vector3::Dot(cA->axisZ, sepN) * cA->radius.z);

    const float lenB =
        fabsf(Vector3::Dot(cB->axisX, sepN) * cB->radius.x) +
        fabsf(Vector3::Dot(cB->axisY, sepN) * cB->radius.y) +
        fabsf(Vector3::Dot(cB->axisZ, sepN) * cB->radius.z);

    const float overlap = (lenA + lenB) - length;

    if (overlap < 0.0f)
    {
        return false;
    }

    // 最小 overlap を保存
    if (overlap < mtv.depth)
    {
        mtv.depth = overlap;
        mtv.axis  = sepN;
        mtv.valid = true;
    }

    return true;
}

bool PhysWorld::IsCollideBoxOBB_MTV(const OBB* cA,
                                    const OBB* cB,
                                    MTVResult& mtv) const
{
    const Vector3 vDistance = cB->pos - cA->pos;

    return
        CompareLengthOBB_MTV(cA, cB, cA->axisX, vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, cA->axisY, vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, cA->axisZ, vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, cB->axisX, vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, cB->axisY, vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, cB->axisZ, vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisX, cB->axisX), vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisX, cB->axisY), vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisX, cB->axisZ), vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisY, cB->axisX), vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisY, cB->axisY), vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisY, cB->axisZ), vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisZ, cB->axisX), vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisZ, cB->axisY), vDistance, mtv) &&
        CompareLengthOBB_MTV(cA, cB, Vector3::Cross(cA->axisZ, cB->axisZ), vDistance, mtv);
}

//=============================================================================
// 押し戻し方向決定
//=============================================================================
Vector3 PhysWorld::ComputePushBackDirection(const ColliderComponent* a,
                                            const ColliderComponent* b,
                                            bool allowY) const
{
    MTVResult mtv;

    auto obbA = a->GetBoundingVolume()->GetOBB();
    auto obbB = b->GetBoundingVolume()->GetOBB();

    if (!IsCollideBoxOBB_MTV(obbA.get(), obbB.get(), mtv) || !mtv.valid)
    {
        // fallback（ほぼ重心差）
        Vector3 d = a->GetPosition() - b->GetPosition();
        if (!allowY)
        {
            d.y = 0.0f;
        }

        if (d.LengthSq() > Math::NearZeroEpsilon)
        {
            d.Normalize();
        }
        else
        {
            d = Vector3::UnitZ;
        }

        return d * 0.01f;
    }

    Vector3 axis = mtv.axis;

    // Y 押し禁止で、ほぼ Y 軸なら無効
    if (!allowY && fabsf(axis.y) > 0.707f)
    {
        return Vector3::Zero;
    }

    if (!allowY)
    {
        axis.y = 0.0f;
        if (axis.LengthSq() < Math::NearZeroEpsilon)
        {
            return Vector3::Zero;
        }
        axis.Normalize();
    }

    // 向きを A から外へ
    const Vector3 dirAB = a->GetPosition() - b->GetPosition();
    if (Vector3::Dot(axis, dirAB) < 0.0f)
    {
        axis *= -1.0f;
    }

    const float kPushEpsXZ = 0.01f;
    const float kPushEpsY  = 0.001f;

    float eps = kPushEpsXZ;
    if (allowY && fabsf(axis.y) > 0.707f)
    {
        eps = kPushEpsY;
    }

    return axis * (mtv.depth + eps);
}

//=============================================================================
// OBB 接触情報（Contact）
//=============================================================================
bool PhysWorld::IntersectOBBContact(const OBB* a,
                                    const OBB* b,
                                    Contact& out) const
{
    out = Contact{};

    MTVResult mtv;
    if (!IsCollideBoxOBB_MTV(a, b, mtv) || !mtv.valid)
    {
        return false;
    }

    Vector3 n = mtv.axis;
    if (n.LengthSq() < Math::NearZeroEpsilon)
    {
        return false;
    }

    const Vector3 dirAB = a->pos - b->pos;
    if (Vector3::Dot(n, dirAB) < 0.0f)
    {
        n *= -1.0f;
    }

    out.hit    = true;
    out.depth  = mtv.depth;
    out.normal = n;

    const float kPushEpsXZ = 0.01f;
    const float kPushEpsY  = 0.001f;

    float eps = (fabsf(n.y) > 0.707f) ? kPushEpsY : kPushEpsXZ;
    out.mtv = n * (mtv.depth + eps);

    return true;
}

} // namespace toy
