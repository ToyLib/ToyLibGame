//=============================================================================
// PhysWorld_Terrain.cpp
//=============================================================================
#include "Physics/PhysWorld.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace toy {

//=============================================================================
// Terrain polygon utilities
//=============================================================================
bool PhysWorld::IsInPolygon(const Polygon* pl, const Vector3& p) const
{
    if (!pl)
    {
        return false;
    }

    // 境界許容（edge 落ち防止）
    const float EPS = 1e-4f;

    auto Cross2D = [](const Vector3& a,
                      const Vector3& b,
                      const Vector3& c) -> float
    {
        const float abx = b.x - a.x;
        const float abz = b.z - a.z;
        const float acx = c.x - a.x;
        const float acz = c.z - a.z;
        return abx * acz - abz * acx;
    };

    const float c0 = Cross2D(pl->a, pl->b, p);
    const float c1 = Cross2D(pl->b, pl->c, p);
    const float c2 = Cross2D(pl->c, pl->a, p);

    const bool hasNeg = (c0 < -EPS) || (c1 < -EPS) || (c2 < -EPS);
    const bool hasPos = (c0 >  EPS) || (c1 >  EPS) || (c2 >  EPS);

    return !(hasNeg && hasPos);
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
// Ground (Terrain only)
//=============================================================================
bool PhysWorld::GetGroundHitAt(const Vector3& pos, GroundHit& outHit) const
{
    outHit = GroundHit{};

    if (mTerrainPolygons.empty())
    {
        return false;
    }

    //============================================================
    // グリッド未使用時：全探索
    //============================================================
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

    //============================================================
    // グリッド使用時（3x3 セル探索）
    //============================================================
    const float minX = mTerrainGrid.origin.x;
    const float minZ = mTerrainGrid.origin.y;
    const float cs   = mTerrainGrid.cellSize;

    const int baseCX =
        static_cast<int>(std::floor((pos.x - minX) / cs));
    const int baseCZ =
        static_cast<int>(std::floor((pos.z - minZ) / cs));

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

            // ★ 修正点：型を明示（1次元 cells 前提）
            const std::vector<int>& candidates =
                mTerrainGrid.cells[mTerrainGrid.CellIndex(cx, cz)];

            for (int idx : candidates)
            {
                if (idx < 0 ||
                    idx >= static_cast<int>(mTerrainPolygons.size()))
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
                    highestY   = y;
                    found      = true;
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

    cellSize = Math::Clamp(cellSize, 1.0f, 50.0f);

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

        int cx0 = Math::Clamp(ToCellX(pMinX), 0, cols - 1);
        int cx1 = Math::Clamp(ToCellX(pMaxX), 0, cols - 1);
        int cz0 = Math::Clamp(ToCellZ(pMinZ), 0, rows - 1);
        int cz1 = Math::Clamp(ToCellZ(pMaxZ), 0, rows - 1);

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
    if (GetNearestGroundHitAtXZ(pos, hit))
    {
        return hit.y;
    }
    return -std::numeric_limits<float>::max();
}

} // namespace toy
