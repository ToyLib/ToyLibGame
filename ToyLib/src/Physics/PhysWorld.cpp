//=============================================================================
// PhysWorld.cpp
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Asset/Geometry/Polygon.h"
#include "Engine/Core/Actor.h"
#include "Movement/MoveComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Utils/MathUtil.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace toy {

//=============================================================================
// Lifecycle
//=============================================================================
PhysWorld::PhysWorld()
{
}

PhysWorld::~PhysWorld()
{
}

//=============================================================================
// Terrain polygon utilities
//=============================================================================

//------------------------------------------------------------------------------
// IsInPolygon
//------------------------------------------------------------------------------
// ・XZ 平面に投影した三角形に点 p が含まれるか判定
// ・辺ごとの“同じ向き”を符号で見る簡易版
// ・※ ポリゴン頂点の並び（CW/CCW）に依存しうるので、
//    現状の地形データ前提で運用（必要ならバリセンタ法等へ差し替え）
//------------------------------------------------------------------------------
bool PhysWorld::IsInPolygon(const Polygon* pl, const Vector3& p) const
{
    // edge AB
    if (((pl->b.x - pl->a.x) * (p.z - pl->a.z) -
         (pl->b.z - pl->a.z) * (p.x - pl->a.x)) < 0.0f)
    {
        return false;
    }

    // edge BC
    if (((pl->c.x - pl->b.x) * (p.z - pl->b.z) -
         (pl->c.z - pl->b.z) * (p.x - pl->b.x)) < 0.0f)
    {
        return false;
    }

    // edge CA
    if (((pl->a.x - pl->c.x) * (p.z - pl->c.z) -
         (pl->a.z - pl->c.z) * (p.x - pl->c.x)) < 0.0f)
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// PolygonHeight
//------------------------------------------------------------------------------
// ・XZ 平面上の点 p に対して、その三角形ポリゴン上の Y 高さを返す
// ・3点から平面方程式を作り、y を解く
// ・※ wc が 0 に近い（ほぼ垂直面）と不安定になり得るので保険フォールバック
//------------------------------------------------------------------------------
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

    // wc が 0 に近い（ほぼ垂直面）場合は不安定になり得る。
    // 地面用途ならまず起きない想定だが、保険として頂点Aの高さを返す。
    if (fabsf(wc) < 1e-6f)
    {
        return pl->a.y;
    }

    return -(wa * (p.x - pl->a.x) + wb * (p.z - pl->a.z)) / wc + pl->a.y;
}

//------------------------------------------------------------------------------
// PolygonNormal
//------------------------------------------------------------------------------
// ・ポリゴンの法線を返す（上向きに揃える）
// ・左手/右手の違いがあっても、「地面の法線」として UnitY と同じ向きに揃える
//------------------------------------------------------------------------------
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

    // “地面の法線”用途：上向きに揃える（坂で裏返るのを防ぐ）
    if (Vector3::Dot(n, Vector3::UnitY) < 0.0f)
    {
        n *= -1.0f;
    }

    return n;
}

//------------------------------------------------------------------------------
// GetGroundHitAt
//------------------------------------------------------------------------------
// ・pos の真下にある TerrainPolygons を走査し、最も高いヒットを返す（Terrainのみ）
// ・mTerrainGrid が有効なら「周囲 3×3 セル」の候補だけを走査して高速化
//------------------------------------------------------------------------------
bool PhysWorld::GetGroundHitAt(const Vector3& pos, GroundHit& outHit) const
{
    outHit = GroundHit{};

    if (mTerrainPolygons.empty())
    {
        return false;
    }

    //--------------------------------------------------------------------------
    // フォールバック：全走査（グリッド無効 / 未構築）
    //--------------------------------------------------------------------------
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

    //--------------------------------------------------------------------------
    // グリッド版：pos が属するセルの「周囲 3×3」候補だけ走査
    //--------------------------------------------------------------------------
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

            if (candidates.empty())
            {
                continue;
            }

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

//------------------------------------------------------------------------------
// SetGroundPolygons
//------------------------------------------------------------------------------
// ・地形ポリゴンを保持し、XZ グリッドで候補を間引くための索引を作る
// ・セルサイズは「平均 targetPolysPerCell くらい」になるように自動決定
//------------------------------------------------------------------------------
void PhysWorld::SetGroundPolygons(const std::vector<Polygon>& polys)
{
    mTerrainPolygons = polys;

    // グリッドを作り直す
    mTerrainGrid.Clear();

    if (mTerrainPolygons.empty())
    {
        return;
    }

    //--------------------------------------------------------------------------
    // 1) 地形全体のXZ範囲を計算
    //--------------------------------------------------------------------------
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

    //--------------------------------------------------------------------------
    // 2) セルサイズを自動決定（平均で targetPolysPerCell くらい）
    //--------------------------------------------------------------------------
    const int targetPolysPerCell = 100;
    const int polyCount = static_cast<int>(mTerrainPolygons.size());

    const int targetCellCount =
        std::max(1, polyCount / targetPolysPerCell);

    const float targetCellArea =
        areaXZ / static_cast<float>(targetCellCount);

    float cellSize =
        std::sqrt(std::max(0.0001f, targetCellArea));

    // 小さすぎ / 大きすぎを防止（好みで調整）
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

    //--------------------------------------------------------------------------
    // 3) 各ポリゴンを「重なるセル全部」に登録（XZ AABB でセル範囲を取る）
    //--------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// GetGroundHeightAt
//------------------------------------------------------------------------------
// ・TerrainPolygons のみ対象で高さだけ返す（見つからない場合は -∞）
//------------------------------------------------------------------------------
float PhysWorld::GetGroundHeightAt(const Vector3& pos) const
{
    GroundHit hit;
    if (GetGroundHitAt(pos, hit))
    {
        return hit.y;
    }

    return -std::numeric_limits<float>::max();
}

//------------------------------------------------------------------------------
// FindFootCollider
//------------------------------------------------------------------------------
// ・Actor が持つ ColliderComponent の中から C_FOOT を持つものを探す
//------------------------------------------------------------------------------
ColliderComponent* PhysWorld::FindFootCollider(const Actor* a) const
{
    for (auto* comp : a->GetAllComponents<ColliderComponent>())
    {
        if (comp->HasFlag(C_FOOT))
        {
            return comp;
        }
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// GetNearestGroundY
//------------------------------------------------------------------------------
// ・互換用：GetNearestGroundHit の薄いラッパ
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// GetNearestGroundHit
//------------------------------------------------------------------------------
// ・Actor の C_FOOT を基準に
//    1) C_GROUND コライダーの上面（XZが重なり、かつ足より下のもの）
//    2) TerrainPolygons の高さ（Actor中心点）
//  を比較して、最も高いものを「地面」として返す
//------------------------------------------------------------------------------
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

    //--------------------------------------------------------------------------
    // 1) C_GROUND コライダー（上面が最も高いもの）
    //--------------------------------------------------------------------------
    for (auto* c : mColliders)
    {
        if (!c->HasFlag(C_GROUND))
        {
            continue;
        }
        if (c->GetOwner() == a)
        {
            continue;
        }

        const Cube other = c->GetBoundingVolume()->GetWorldAABB();

        //========================================================
        // ★床の端で「1フレーム粘る」対策：
        //   足AABBを少し縮めて xzOverlap を判定する
        //========================================================
        const float kGroundEdgeEps = 0.08f; // 0.03〜0.15 目安（まず 0.08）
        const bool xzOverlap =
            (box.max.x - kGroundEdgeEps > other.min.x) && (box.min.x + kGroundEdgeEps < other.max.x) &&
            (box.max.z - kGroundEdgeEps > other.min.z) && (box.min.z + kGroundEdgeEps < other.max.z);

        if (!xzOverlap)
        {
            continue;
        }

        //========================================================
        // ★床候補の“上面”は「足元より少し上まで」しか許さない
        //========================================================
        const float kFootAllowance = 0.25f;   // 既に値があるならそれを使う
        if (other.max.y > footY + kFootAllowance)
        {
            continue;
        }

        // 「足より下〜少し上」だけに絞った上で、最も高いものを採用
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

    //--------------------------------------------------------------------------
    // 2) TerrainPolygons（Actor中心点）
    //--------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// CompareLengthOBB (SAT判定用)
//------------------------------------------------------------------------------
// ・vSep を分離軸として、距離(vDistanceの投影) と AABB投影長(A+B) を比較
// ・true なら「分離していない（重なっている可能性）」
//------------------------------------------------------------------------------
bool PhysWorld::CompareLengthOBB(const OBB* cA,
                                 const OBB* cB,
                                 const Vector3& vSep,
                                 const Vector3& vDistance) const
{
    const float kAxisEps = 1e-6f;
    const float lenSq = vSep.LengthSq();
    if (lenSq < kAxisEps)
    {
        return true; // 分離軸として無効（平行で外積が死んだ等）
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    const Vector3 sepN = vSep * invLen; // ★正規化

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

    // Aの軸
    if (!CompareLengthOBB(cA, cB, cA->axisX, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cA->axisY, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cA->axisZ, vDistance)) { return false; }

    // Bの軸
    if (!CompareLengthOBB(cA, cB, cB->axisX, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cB->axisY, vDistance)) { return false; }
    if (!CompareLengthOBB(cA, cB, cB->axisZ, vDistance)) { return false; }

    // 外積軸
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

//------------------------------------------------------------------------------
// CompareLengthOBB_MTV
//------------------------------------------------------------------------------
// ・SAT軸に対する overlap（めり込み量）を計算し、最小 overlap を MTV として保持する
// ・重要：軸を正規化して overlap を「距離」として扱えるようにする
//   （正規化しないと軸長で overlap がスケールし、押し戻しが暴れやすい）
//------------------------------------------------------------------------------
bool PhysWorld::CompareLengthOBB_MTV(const OBB* cA,
                                    const OBB* cB,
                                    const Vector3& vSep,
                                    const Vector3& vDistance,
                                    MTVResult& mtv) const
{
    const float kAxisEps = 1e-6f;
    const float lenSq = vSep.LengthSq();

    // ほぼゼロ軸（外積が潰れるケースなど）は無視
    if (lenSq < kAxisEps)
    {
        return true;
    }

    // ★正規化して「距離としての overlap」を得る
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

    // overlap < 0 なら分離（=非衝突）
    if (overlap < 0.0f)
    {
        return false;
    }

    // 最小 overlap を更新
    if (overlap < mtv.depth)
    {
        mtv.depth = overlap;
        mtv.axis  = sepN;   // ★正規化軸を保存
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

//------------------------------------------------------------------------------
// ComputePushBackDirection
//------------------------------------------------------------------------------
// ・OBB×OBB の MTV を元に「押し戻し量（距離）」を返す
// ・allowY=false のときは水平面（XZ）だけで押し戻す
//------------------------------------------------------------------------------
Vector3 PhysWorld::ComputePushBackDirection(const ColliderComponent* a,
                                           const ColliderComponent* b,
                                           bool allowY) const
{
    MTVResult mtv;

    auto obb1 = a->GetBoundingVolume()->GetOBB();
    auto obb2 = b->GetBoundingVolume()->GetOBB();

    if (!IsCollideBoxOBB_MTV(obb1.get(), obb2.get(), mtv) || !mtv.valid)
    {
        // フォールバック：距離ベクトルから押し戻し方向を作る
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

    // mtv.axis は CompareLengthOBB_MTV で正規化済み（※ allowY=false でYを潰す場合は再正規化が必要）
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

    // a の視点から見て外側へ向くように補正
    const Vector3 dirAB = a->GetPosition() - b->GetPosition();
    if (Vector3::Dot(pushAxis, dirAB) < 0.0f)
    {
        pushAxis *= -1.0f;
    }

    // 押し戻し距離（小さい余裕を足して“再めり込み”を減らす）
    const float kPushEps = 0.05f;
    return pushAxis * (mtv.depth + kPushEps);
}

//=============================================================================
// Depenetration helpers (insurance)
//=============================================================================

//------------------------------------------------------------------------------
// ResolveCeiling
//------------------------------------------------------------------------------
// ・moverFlag を持つ Actor のコライダーが ceilingFlag に“めり込んでいたら”
//   下向き成分だけ押し戻しベクトルとして合算して返す
//------------------------------------------------------------------------------
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
        if (!c1->HasFlag(moverFlag))
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
            if (!c2->HasFlag(ceilingFlag))
            {
                continue;
            }

            if (!(JudgeWithRadius(c1, c2) && JudgeWithOBB(c1, c2)))
            {
                continue;
            }

            // 天井なので allowY = true
            const Vector3 push = ComputePushBackDirection(c1, c2, true);

            // “天井ヒット”として扱うのは「下に押される」成分のみ
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

//------------------------------------------------------------------------------
// IntersectRayOBB
//------------------------------------------------------------------------------
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

        if (i == 0)
        {
            axis = obb->axisX;
            r = obb->radius.x;
        }
        else if (i == 1)
        {
            axis = obb->axisY;
            r = obb->radius.y;
        }
        else
        {
            axis = obb->axisZ;
            r = obb->radius.z;
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
            // Ray が軸にほぼ平行：中心投影がスラブ外なら非交差
            if (-e - r > 0.0f || -e + r < 0.0f)
            {
                return false;
            }
        }
    }

    outT = tMin;
    return true;
}

//------------------------------------------------------------------------------
// RayHitWall
//------------------------------------------------------------------------------
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
        if (!col->HasFlag(C_WALL))
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

    // ほんの少し手前で止めてめり込み防止
    hitPos = ray.start + ray.dir * (closestT - 0.01f);
    return true;
}

//------------------------------------------------------------------------------
// Raycast
//------------------------------------------------------------------------------
bool PhysWorld::Raycast(const Vector3& origin,
                       const Vector3& dir,
                       float maxDist,
                       uint32_t flagMask,
                       RaycastHit& outHit) const
{
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
        if (!(col->GetFlags() & flagMask))
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
                outHit.actor    = col->GetOwner();
                outHit.distance = t;
                outHit.point    = ray.start + ray.dir * t;
            }
        }
    }

    return hit;
}


void PhysWorld::QueryView(const ViewQueryDesc& desc,
                          std::vector<ViewQueryHit>& outHits) const
{
    outHits.clear();

    // sanitize
    Vector3 fwd = desc.forward;
    if (fwd.LengthSq() < Math::NearZeroEpsilon)
    {
        return;
    }
    fwd.Normalize();

    const float maxDist = std::max(0.0f, desc.maxDist);
    const float maxDistSq = maxDist * maxDist;

    // halfFov の cos を閾値にする（dot >= cosHalfFov が視界内）
    const float halfFov = desc.fovRad * 0.5f;
    const float cosHalfFov = std::cos(halfFov);

    for (auto* col : mColliders)
    {
        if (!col->GetEnabled())
        {
            continue;
        }
        if (!(col->GetFlags() & desc.flagMask))
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

        // ターゲット点は「コライダー中心」寄りの方が安定
        // すでに OBB.pos がワールド中心を持ってるのでそれを使う
        Vector3 targetPos;
        if (auto obb = col->GetBoundingVolume()->GetOBB())
        {
            targetPos = obb->pos;
        }
        else
        {
            // フォールバック：Actor位置
            targetPos = owner->GetPosition();
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
            // origin と同位置なら「視界内扱い」でOK
            to = fwd;
            dist = 0.0f;
        }

        const float cosAng = Vector3::Dot(fwd, to);
        if (cosAng < cosHalfFov)
        {
            continue;
        }

        // 遮蔽チェック（必要なときだけ）
        if (desc.requireLOS)
        {
            RaycastHit hit;
            // origin から target 方向へ、target までの距離で Raycast
            // ※ self が当たるのを避けたいなら Raycast に ignoreActor を足すのが理想
            if (Raycast(desc.origin, to, dist, desc.losBlockMask, hit))
            {
                // 壁に先に当たった＝見えてない
                // （hit.distance < dist を厳密に見る）
                if (hit.distance + 0.02f < dist)
                {
                    continue;
                }
            }
        }

        ViewQueryHit h;
        h.actor = owner;
        h.collider = col;
        h.dist = dist;
        h.cosAngle = cosAng;
        outHits.emplace_back(h);
    }

    // kit側で使いやすいように「近い順 + 中央寄り」を軽く整列しておく案
    // ただし “選び方” は kit の責務なので、ここでは並べ替えない方針でもOK
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

//=============================================================================
// Pair scan / callbacks
//=============================================================================

//------------------------------------------------------------------------------
// CollideAndCallback
//------------------------------------------------------------------------------
// ・flagA と flagB の組み合わせを総当たりで判定し、Collided() を呼ぶ
// ・doPushBack=true なら c1(=flagA側) を押し戻す
//------------------------------------------------------------------------------
void PhysWorld::CollideAndCallback(uint32_t flagA,
                                  uint32_t flagB,
                                  bool doPushBack,
                                  bool allowY,
                                  bool stopVerticalSpeed)
{
    for (auto* c1 : mColliders)
    {
        if (!c1->GetEnabled() || !c1->HasFlag(flagA))
        {
            continue;
        }

        Vector3 totalPush = Vector3::Zero;
        bool collided = false;

        for (auto* c2 : mColliders)
        {
            if (!c2->GetEnabled() || !c2->HasFlag(flagB))
            {
                continue;
            }
            if (c1->GetOwner() == c2->GetOwner())
            {
                continue;
            }

            // Sphere で早期判定 → OBB で精密判定
            if (JudgeWithRadius(c1, c2) && JudgeWithOBB(c1, c2))
            {
                c1->Collided(c2);
                c2->Collided(c1);
                collided = true;

                if (doPushBack)
                {
                    totalPush += ComputePushBackDirection(c1, c2, allowY);
                }
            }
        }

        if (collided && doPushBack)
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

//------------------------------------------------------------------------------
// Test
//------------------------------------------------------------------------------
void PhysWorld::Test()
{
    // まず全コライダーのヒットバッファをクリア
    for (auto* c : mColliders)
    {
        c->ClearCollidBuffer();
    }

    // 通常の衝突（OBB + Sphere）
    CollideAndCallback(C_PLAYER, C_ENEMY);
    CollideAndCallback(C_PLAYER, C_BULLET);
    CollideAndCallback(C_ENEMY,  C_WALL, true, false);

    //--------------------------------------------------------------------------
    // Laser vs Enemy（Ray vs Mesh）
    //--------------------------------------------------------------------------
    for (auto* c1 : mColliders)
    {
        if (!c1->HasFlag(C_LASER) || !c1->GetEnabled())
        {
            continue;
        }

        const Ray ray = c1->GetRay();

        for (auto* c2 : mColliders)
        {
            if (c1 == c2)
            {
                continue;
            }
            if (!c2->HasFlag(C_ENEMY) || !c2->GetEnabled())
            {
                continue;
            }

            const auto& polygons = c2->GetBoundingVolume()->GetPolygons();

            bool hit = false;
            float closestT = Math::Infinity;
            Vector3 hitPoint = Vector3::Zero;

            for (size_t i = 0; i < polygons.size(); ++i)
            {
                const auto& poly = polygons[i];

                float t;
                if (IntersectRayTriangle(ray, poly.a, poly.b, poly.c, t))
                {
                    if (t < closestT)
                    {
                        closestT = t;
                        hit = true;
                        hitPoint = ray.start + ray.dir * t;
                    }
                }
            }

            if (hit)
            {
                c1->Collided(c2);
                c2->Collided(c1);
                (void)hitPoint; // エフェクト等に使うならここで利用
            }
        }
    }
}

} // namespace toy
