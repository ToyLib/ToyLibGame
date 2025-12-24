//=============================================================================
// PhysWorld.cpp
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Movement/MoveComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace toy {

//=============================================================================
// Ray vs Triangle (Möller–Trumbore)
//=============================================================================
bool IntersectRayTriangle(
    const Ray& ray,
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
// Lifecycle
//=============================================================================
PhysWorld::PhysWorld() {}
PhysWorld::~PhysWorld() {}

//=============================================================================
// Terrain polygon utilities
//=============================================================================
bool PhysWorld::IsInPolygon(const Polygon* pl, const Vector3& p) const
{
    if (((pl->b.x - pl->a.x) * (p.z - pl->a.z) -
         (pl->b.z - pl->a.z) * (p.x - pl->a.x)) < 0.0f)
    {
        return false;
    }
    if (((pl->c.x - pl->b.x) * (p.z - pl->b.z) -
         (pl->c.z - pl->b.z) * (p.x - pl->b.x)) < 0.0f)
    {
        return false;
    }
    if (((pl->a.x - pl->c.x) * (p.z - pl->c.z) -
         (pl->a.z - pl->c.z) * (p.x - pl->c.x)) < 0.0f)
    {
        return false;
    }
    return true;
}

float PhysWorld::PolygonHeight(const Polygon* pl, const Vector3& p) const
{
    const float wa =
        (pl->c.z - pl->a.z) * (pl->b.y - pl->a.y) -
        (pl->c.y - pl->a.y) * (pl->b.z - pl->a.z);

    const float wb =
        (pl->c.y - pl->a.y) * (pl->b.x - pl->a.x) -
        (pl->c.x - pl->a.x) * (pl->b.y - pl->a.y);

    const float wc =
        (pl->c.x - pl->a.x) * (pl->b.z - pl->a.z) -
        (pl->c.z - pl->a.z) * (pl->b.x - pl->a.x);

    if (fabsf(wc) < 1e-6f)
    {
        return pl->a.y;
    }

    return -(wa * (p.x - pl->a.x) + wb * (p.z - pl->a.z)) / wc + pl->a.y;
}

Vector3 PhysWorld::PolygonNormal(const Polygon& pl) const
{
    const Vector3 e1 = pl.b - pl.a;
    const Vector3 e2 = pl.c - pl.a;

    Vector3 n = Vector3::Cross(e1, e2);

    if (n.LengthSq() > Math::NearZeroEpsilon)
    {
        n.Normalize();
    }
    else
    {
        n = Vector3::UnitY;
    }

    if (Vector3::Dot(n, Vector3::UnitY) < 0.0f)
    {
        n *= -1.0f;
    }

    return n;
}

//=============================================================================
// Ground
//=============================================================================
bool PhysWorld::GetGroundHitAt(const Vector3& pos, GroundHit& outHit) const
{
    outHit = GroundHit{};

    if (mTerrainPolygons.empty())
    {
        return false;
    }

    // fallback: full scan
    if (!mTerrainGrid.enabled || mTerrainGrid.cells.empty())
    {
        float highestY = -std::numeric_limits<float>::max();
        bool found = false;

        for (const auto& poly : mTerrainPolygons)
        {
            if (!IsInPolygon(&poly, pos))
            {
                continue;
            }

            const float y = PolygonHeight(&poly, pos);
            if (y > highestY)
            {
                highestY = y;
                found = true;

                outHit.hit      = true;
                outHit.y        = y;
                outHit.pos      = Vector3(pos.x, y, pos.z);
                outHit.normal   = PolygonNormal(poly);
                outHit.source   = GroundSource::Terrain;
                outHit.collider = nullptr;
            }
        }

        return found;
    }

    // grid scan (3x3)
    const float minX = mTerrainGrid.origin.x;
    const float minZ = mTerrainGrid.origin.y;
    const float cs   = mTerrainGrid.cellSize;

    const int baseCX = static_cast<int>(std::floor((pos.x - minX) / cs));
    const int baseCZ = static_cast<int>(std::floor((pos.z - minZ) / cs));

    float highestY = -std::numeric_limits<float>::max();
    bool found = false;
    Vector3 bestNormal = Vector3::UnitY;

    for (int dz = -1; dz <= 1; ++dz)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const int cx = baseCX + dx;
            const int cz = baseCZ + dz;

            if (!mTerrainGrid.IsValidCell(cx, cz))
            {
                continue;
            }

            const auto& candidates =
                mTerrainGrid.cells[mTerrainGrid.CellIndex(cx, cz)];

            for (int idx : candidates)
            {
                if (idx < 0 || idx >= static_cast<int>(mTerrainPolygons.size()))
                {
                    continue;
                }

                const auto& poly = mTerrainPolygons[idx];

                if (!IsInPolygon(&poly, pos))
                {
                    continue;
                }

                const float y = PolygonHeight(&poly, pos);
                if (y > highestY)
                {
                    highestY = y;
                    found = true;
                    bestNormal = PolygonNormal(poly);
                }
            }
        }
    }

    if (!found)
    {
        return false;
    }

    outHit.hit      = true;
    outHit.y        = highestY;
    outHit.pos      = Vector3(pos.x, highestY, pos.z);
    outHit.normal   = bestNormal;
    outHit.source   = GroundSource::Terrain;
    outHit.collider = nullptr;

    return true;
}

void PhysWorld::SetGroundPolygons(const std::vector<Polygon>& polys)
{
    mTerrainPolygons = polys;
    mTerrainGrid.Clear();

    if (mTerrainPolygons.empty())
    {
        return;
    }

    float minX =  std::numeric_limits<float>::max();
    float minZ =  std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    for (const auto& p : mTerrainPolygons)
    {
        minX = std::min(minX, std::min({ p.a.x, p.b.x, p.c.x }));
        minZ = std::min(minZ, std::min({ p.a.z, p.b.z, p.c.z }));
        maxX = std::max(maxX, std::max({ p.a.x, p.b.x, p.c.x }));
        maxZ = std::max(maxZ, std::max({ p.a.z, p.b.z, p.c.z }));
    }

    const float width  = std::max(0.001f, maxX - minX);
    const float depth  = std::max(0.001f, maxZ - minZ);
    const float areaXZ = width * depth;

    const int targetPolysPerCell = 100;
    const int polyCount = static_cast<int>(mTerrainPolygons.size());

    const int targetCellCount =
        std::max(1, polyCount / targetPolysPerCell);

    const float targetCellArea =
        areaXZ / static_cast<float>(targetCellCount);

    float cellSize =
        std::sqrt(std::max(0.0001f, targetCellArea));

    const float minCell = 1.0f;
    const float maxCell = 50.0f;
    cellSize = Math::Clamp(cellSize, minCell, maxCell);

    const int cols = std::max(1, static_cast<int>(std::ceil(width / cellSize)));
    const int rows = std::max(1, static_cast<int>(std::ceil(depth / cellSize)));

    mTerrainGrid.enabled  = true;
    mTerrainGrid.origin   = Vector2(minX, minZ);
    mTerrainGrid.cellSize = cellSize;
    mTerrainGrid.cols     = cols;
    mTerrainGrid.rows     = rows;
    mTerrainGrid.cells.resize(static_cast<size_t>(cols * rows));

    auto ToCellX = [&](float x) -> int
    {
        return static_cast<int>(std::floor((x - minX) / cellSize));
    };

    auto ToCellZ = [&](float z) -> int
    {
        return static_cast<int>(std::floor((z - minZ) / cellSize));
    };

    for (int i = 0; i < polyCount; ++i)
    {
        const auto& p = mTerrainPolygons[i];

        const float pMinX = std::min({ p.a.x, p.b.x, p.c.x });
        const float pMaxX = std::max({ p.a.x, p.b.x, p.c.x });
        const float pMinZ = std::min({ p.a.z, p.b.z, p.c.z });
        const float pMaxZ = std::max({ p.a.z, p.b.z, p.c.z });

        int cx0 = ToCellX(pMinX);
        int cx1 = ToCellX(pMaxX);
        int cz0 = ToCellZ(pMinZ);
        int cz1 = ToCellZ(pMaxZ);

        cx0 = Math::Clamp(cx0, 0, cols - 1);
        cx1 = Math::Clamp(cx1, 0, cols - 1);
        cz0 = Math::Clamp(cz0, 0, rows - 1);
        cz1 = Math::Clamp(cz1, 0, rows - 1);

        for (int cz = cz0; cz <= cz1; ++cz)
        {
            for (int cx = cx0; cx <= cx1; ++cx)
            {
                mTerrainGrid.cells[mTerrainGrid.CellIndex(cx, cz)].push_back(i);
            }
        }
    }
}

float PhysWorld::GetGroundHeightAt(const Vector3& pos) const
{
    GroundHit hit;
    if (GetGroundHitAt(pos, hit))
    {
        return hit.y;
    }
    return -std::numeric_limits<float>::max();
}

ColliderComponent* PhysWorld::FindFootCollider(const Actor* a) const
{
    for (auto* comp : a->GetAllComponents<ColliderComponent>())
    {
        if (comp->HasAnyFlag(C_FOOT))
        {
            return comp;
        }
    }
    return nullptr;
}

bool PhysWorld::GetNearestGroundY(const Actor* a, float& outY) const
{
    GroundHit hit;
    if (!GetNearestGroundHit(a, hit))
    {
        return false;
    }
    outY = hit.y;
    return true;
}

bool PhysWorld::GetNearestGroundHit(const Actor* a, GroundHit& outHit) const
{
    outHit = GroundHit{};

    if (!a)
    {
        return false;
    }

    const auto* foot = FindFootCollider(a);
    if (!foot)
    {
        return false;
    }

    const Cube  box   = foot->GetBoundingVolume()->GetWorldAABB();
    const float footY = box.min.y;

    float highest = -std::numeric_limits<float>::max();
    bool found = false;

    GroundSource bestSource = GroundSource::None;
    const ColliderComponent* bestCol = nullptr;
    Vector3 bestPos = Vector3::Zero;
    Vector3 bestNormal = Vector3::UnitY;

    // 1) C_GROUND colliders
    for (auto* c : mColliders)
    {
        if (!c->HasAnyFlag(C_GROUND))
        {
            continue;
        }
        if (c->GetOwner() == a)
        {
            continue;
        }

        const Cube other = c->GetBoundingVolume()->GetWorldAABB();

        const float kGroundEdgeEps = 0.08f;
        const bool xzOverlap =
            (box.max.x - kGroundEdgeEps > other.min.x) && (box.min.x + kGroundEdgeEps < other.max.x) &&
            (box.max.z - kGroundEdgeEps > other.min.z) && (box.min.z + kGroundEdgeEps < other.max.z);

        if (!xzOverlap)
        {
            continue;
        }

        const float kFootAllowance = 0.25f;
        if (other.max.y > footY + kFootAllowance)
        {
            continue;
        }

        if (other.max.y > highest)
        {
            highest = other.max.y;
            found = true;

            bestSource = GroundSource::Collider;
            bestCol = c;

            bestPos = Vector3((box.min.x + box.max.x) * 0.5f,
                              highest,
                              (box.min.z + box.max.z) * 0.5f);

            bestNormal = Vector3::UnitY;
        }
    }

    // 2) Terrain polygons (actor center)
    const Vector3 center = a->GetPosition();

    GroundHit terrainHit;
    if (GetGroundHitAt(center, terrainHit))
    {
        if (terrainHit.y > highest)
        {
            highest = terrainHit.y;
            found = true;

            bestSource = GroundSource::Terrain;
            bestCol = nullptr;
            bestPos = terrainHit.pos;
            bestNormal = terrainHit.normal;
        }
    }

    if (!found)
    {
        return false;
    }

    outHit.hit      = true;
    outHit.y        = highest;
    outHit.pos      = bestPos;
    outHit.normal   = bestNormal;
    outHit.source   = bestSource;
    outHit.collider = bestCol;
    outHit.yGap     = footY - highest;

    return true;
}

//=============================================================================
// OBB / Sphere collision
//=============================================================================
bool PhysWorld::CompareLengthOBB(const OBB* cA,
                                 const OBB* cB,
                                 const Vector3& vSep,
                                 const Vector3& vDistance) const
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

    if (!CompareLengthOBB(cA, cB, cA->axisX, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cA->axisY, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cA->axisZ, vDistance)) { return false; }

    if (!CompareLengthOBB(cA, cB, cB->axisX, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cB->axisY, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cB->axisZ, vDistance)) { return false; }

    Vector3 vSep;

    vSep = Vector3::Cross(cA->axisX, cB->axisX);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    vSep = Vector3::Cross(cA->axisX, cB->axisY);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    vSep = Vector3::Cross(cA->axisX, cB->axisZ);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    vSep = Vector3::Cross(cA->axisY, cB->axisX);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    vSep = Vector3::Cross(cA->axisY, cB->axisY);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    vSep = Vector3::Cross(cA->axisY, cB->axisZ);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    vSep = Vector3::Cross(cA->axisZ, cB->axisX);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    vSep = Vector3::Cross(cA->axisZ, cB->axisY);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    vSep = Vector3::Cross(cA->axisZ, cB->axisZ);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) { return false; }

    return true;
}

bool PhysWorld::JudgeWithRadius(const ColliderComponent* col1,
                               const ColliderComponent* col2) const
{
    const Vector3 distance = col1->GetPosition() - col2->GetPosition();
    const float len = distance.Length();

    const float threshold =
        col1->GetBoundingVolume()->GetRadius() +
        col2->GetBoundingVolume()->GetRadius();

    return (threshold > len);
}

//=============================================================================
// MTV (push back)
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

Vector3 PhysWorld::ComputePushBackDirection(const ColliderComponent* a,
                                           const ColliderComponent* b,
                                           bool allowY) const
{
    MTVResult mtv;

    auto obb1 = a->GetBoundingVolume()->GetOBB();
    auto obb2 = b->GetBoundingVolume()->GetOBB();

    if (!IsCollideBoxOBB_MTV(obb1.get(), obb2.get(), mtv) || !mtv.valid)
    {
        Vector3 delta = a->GetPosition() - b->GetPosition();
        if (!allowY)
        {
            delta.y = 0.0f;
        }

        if (delta.LengthSq() > Math::NearZeroEpsilon)
        {
            delta.Normalize();
        }
        else
        {
            delta = Vector3::UnitZ;
        }

        return delta * 0.1f;
    }

    Vector3 pushAxis = mtv.axis;

    if (!allowY)
    {
        pushAxis.y = 0.0f;
        if (pushAxis.LengthSq() > Math::NearZeroEpsilon)
        {
            pushAxis.Normalize();
        }
        else
        {
            return Vector3::Zero;
        }
    }

    const Vector3 dirAB = a->GetPosition() - b->GetPosition();
    if (Vector3::Dot(pushAxis, dirAB) < 0.0f)
    {
        pushAxis *= -1.0f;
    }

    const float kPushEps = 0.05f;
    return pushAxis * (mtv.depth + kPushEps);
}

//=============================================================================
// Depenetration helpers
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

    bool hitAny = false;

    for (auto* c1 : mColliders)
    {
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

        for (auto* c2 : mColliders)
        {
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

            if (!(JudgeWithRadius(c1, c2) && JudgeWithOBB(c1, c2)))
            {
                continue;
            }

            const Vector3 push = ComputePushBackDirection(c1, c2, true);

            if (push.y < -0.0001f)
            {
                outPush += push;
                hitAny = true;
            }
        }
    }

    return hitAny;
}

//=============================================================================
// Ray utilities
//=============================================================================
bool PhysWorld::IntersectRayOBB(const Ray& ray, const OBB* obb, float& outT) const
{
    const float epsilon = 1e-6f;

    const Vector3 p = obb->pos - ray.start;

    float tMin = 0.0f;
    float tMax = Math::Infinity;

    for (int i = 0; i < 3; ++i)
    {
        Vector3 axis = Vector3::Zero;
        float r = 0.0f;

        if (i == 0)      { axis = obb->axisX; r = obb->radius.x; }
        else if (i == 1) { axis = obb->axisY; r = obb->radius.y; }
        else             { axis = obb->axisZ; r = obb->radius.z; }

        const float e = Vector3::Dot(axis, p);
        const float f = Vector3::Dot(ray.dir, axis);

        if (fabsf(f) > epsilon)
        {
            float t1 = (e + r) / f;
            float t2 = (e - r) / f;

            if (t1 > t2) { std::swap(t1, t2); }

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

        float t;
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

    float closestT = maxDist;
    bool hit = false;

    for (auto* col : mColliders)
    {
        if (!col->GetEnabled())
        {
            continue;
        }

        // Any 判定
        if ((col->GetFlags() & flagMask) == 0)
        {
            continue;
        }

        auto obb = col->GetBoundingVolume()->GetOBB();
        if (!obb)
        {
            continue;
        }

        float t;
        if (IntersectRayOBB(ray, obb.get(), t))
        {
            if (t >= 0.0f && t <= closestT)
            {
                closestT        = t;
                hit             = true;
                outHit.hit      = true;
                outHit.actor    = col->GetOwner();
                outHit.distance = t;
                outHit.point    = ray.start + ray.dir * t;
            }
        }
    }

    return hit;
}

//=============================================================================
// Sensor query
//=============================================================================
void PhysWorld::QueryView(const ViewQueryDesc& desc,
                          std::vector<ViewQueryHit>& outHits) const
{
    outHits.clear();

    // sanitize forward
    Vector3 fwd = desc.forward;
    if (fwd.LengthSq() < Math::NearZeroEpsilon)
    {
        return;
    }
    fwd.Normalize();

    const float maxDist   = std::max(0.0f, desc.maxDist);
    const float maxDistSq = maxDist * maxDist;

    const float halfFov     = desc.fovRad * 0.5f;
    const float cosHalfFov  = std::cos(halfFov);

    const float nearD   = std::max(0.0f, desc.nearOverrideDist);
    const float nearDSq = nearD * nearD;

    for (auto* col : mColliders)
    {
        if (!col || !col->GetEnabled())
        {
            continue;
        }

        // Any: (flags & mask) != 0
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

        // ターゲット点（OBB中心が取れるならそれ）
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
            to *= (1.0f / dist); // normalize
        }
        else
        {
            // 同位置ならとりあえず視界内扱い
            to = fwd;
            dist = 0.0f;
        }

        const bool inNear = (nearDSq > 0.0f) && (distSq <= nearDSq);

        const float cosAng = Vector3::Dot(fwd, to);
        const bool inFov = (cosAng >= cosHalfFov);

        // ★視界 or 近接
        if (!inFov && !inNear)
        {
            continue;
        }

        // LOS（遮蔽）
        if (desc.requireLOS)
        {
            const bool needLOS = !inNear || desc.nearOverrideRequireLOS;
            if (needLOS && desc.losBlockMask != 0)
            {
                // Raycast をそのまま使うと、
                // ターゲット自身が losBlockMask を持ってる場合に「自分で遮蔽」しやすい。
                // なのでここは “ブロッカーだけ” を自前で見る（owner は除外）。
                Ray ray;
                ray.start = desc.origin;
                ray.dir   = to; // normalized

                bool blocked = false;

                for (auto* blockCol : mColliders)
                {
                    if (!blockCol || !blockCol->GetEnabled())
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

                    // ★自分は遮蔽にしない
                    if (desc.ignoreActor && blockOwner == desc.ignoreActor)
                    {
                        continue;
                    }
                    // ★ターゲット自身（同Actor）は遮蔽にしない
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
                        // 0..dist の範囲にヒットしたら遮蔽
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
//=============================================================================
// Collider management
//=============================================================================
void PhysWorld::AddCollider(ColliderComponent* c)
{
    mColliders.emplace_back(c);
}

void PhysWorld::RemoveCollider(ColliderComponent* c)
{
    auto iter = std::find(mColliders.begin(), mColliders.end(), c);
    if (iter != mColliders.end())
    {
        mColliders.erase(iter);
    }
}

void PhysWorld::GetCollidersByAnyFlags(uint32_t mask,
                                       std::vector<ColliderComponent*>& out) const
{
    for (auto* col : mColliders)
    {
        if (!col) continue;
        if (!col->GetEnabled()) continue;

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
        if (!col) continue;
        if (!col->GetEnabled()) continue;

        if ((col->GetFlags() & mask) == mask)
        {
            out.push_back(col);
        }
    }
}

//=============================================================================
// Pair scan / callbacks
//=============================================================================
void PhysWorld::CollideAndCallback(uint32_t flagA,
                                  uint32_t flagB,
                                  bool doPushBack,
                                  bool allowY,
                                  bool stopVerticalSpeed)
{
    for (auto* c1 : mColliders)
    {
        if (!c1->GetEnabled() || !c1->HasAnyFlag(flagA))
        {
            continue;
        }

        Vector3 totalPush = Vector3::Zero;
        bool collided = false;
        bool didPush = false;

        for (auto* c2 : mColliders)
        {
            if (!c2->GetEnabled() || !c2->HasAnyFlag(flagB))
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
                collided = true;

                const bool doPush =
                    doPushBack &&
                    !c1->IsTrigger() &&
                    !c2->IsTrigger();

                if (doPush)
                {
                    totalPush += ComputePushBackDirection(c1, c2, allowY);
                    didPush = true;
                }
            }
        }

        if (collided && didPush)
        {
            c1->GetOwner()->SetPosition(c1->GetOwner()->GetPosition() + totalPush);

            if (stopVerticalSpeed)
            {
                if (auto* move = c1->GetOwner()->GetComponent<MoveComponent>())
                {
                    move->SetVerticalSpeed(0.0f);
                }
            }
        }
    }
}

void PhysWorld::Test()
{
    for (auto* c : mColliders)
    {
        c->ClearCollidBuffer();
    }

    // Move vs Wall
    CollideAndCallback(C_PLAYER_TEAM, C_WALL, true, false);
    CollideAndCallback(C_ENEMY_TEAM,  C_WALL, true, false);

    // Char vs Char (optional)
    CollideAndCallback(C_PLAYER_TEAM, C_ENEMY_TEAM);

    // Combat: HITBOX -> HURTBOX (All)
    auto HasAll = [](const ColliderComponent* c, uint32_t mask)
    {
        return c && c->GetEnabled() && ((c->GetFlags() & mask) == mask);
    };

    // Player攻撃 -> Enemy被弾
    {
        const uint32_t A = C_PLAYER_TEAM | C_HITBOX;
        const uint32_t B = C_ENEMY_TEAM  | C_HURTBOX;

        for (auto* c1 : mColliders)
        {
            if (!HasAll(c1, A)) continue;

            for (auto* c2 : mColliders)
            {
                if (!HasAll(c2, B)) continue;
                if (c1->GetOwner() == c2->GetOwner()) continue;

                if (JudgeWithRadius(c1, c2) && JudgeWithOBB(c1, c2))
                {
                    c1->Collided(c2);
                    c2->Collided(c1);
                }
            }
        }
    }
}

} // namespace toy
