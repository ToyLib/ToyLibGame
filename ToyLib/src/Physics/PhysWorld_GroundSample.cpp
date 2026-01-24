//=============================================================================
// PhysWorld_GroundSample.cpp  (NEW)
//=============================================================================
#include "Physics/PhysWorld.h"

#include "Physics/ColliderComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace toy {

static float DegToRad(float deg) { return deg * 3.1415926535f / 180.0f; }

//-----------------------------------------------------------------------------
// Foot sample points: OBB姿勢を考慮した「下面5点」（＋型）
//-----------------------------------------------------------------------------
void PhysWorld::BuildFootSamplePoints(const ColliderComponent* foot,
                                      std::vector<Vector3>& outPoints) const
{
    outPoints.clear();
    if (!foot) return;

    auto obb = foot->GetBoundingVolume()->GetOBB();
    if (!obb) return;

    // OBB下面中心
    const Vector3 bottomCenter = obb->pos - obb->axisY * obb->radius.y;

    const float rx = std::max(0.0f, obb->radius.x - mFootSampleInset);
    const float rz = std::max(0.0f, obb->radius.z - mFootSampleInset);

    // + 型：中心、左右、前後（OBB軸で）
    outPoints.push_back(bottomCenter);
    outPoints.push_back(bottomCenter + obb->axisX * rx);
    outPoints.push_back(bottomCenter - obb->axisX * rx);
    outPoints.push_back(bottomCenter + obb->axisZ * rz);
    outPoints.push_back(bottomCenter - obb->axisZ * rz);
}

//-----------------------------------------------------------------------------
// サンプル点1つで「一番高い地面」を拾う（Collider床 + Terrain）
//  - startY から maxDist だけ下を見る（垂直）
//  - 地面は “startY より下” の候補のみ採用
//-----------------------------------------------------------------------------
bool PhysWorld::SampleGroundAtPoint(const Vector3& samplePos,
                                    float startY,
                                    float maxDist,
                                    uint32_t groundMask,
                                    const ColliderComponent* ignore,
                                    GroundHit& outHit) const
{
    outHit = GroundHit{};

    const float yMin = startY - maxDist;

    // 候補のうち「最も高い y」を採用
    float bestY = -std::numeric_limits<float>::max();
    GroundHit bestHit;

    //==============================
    // (A) Collider床（C_GROUND）
    //==============================
    for (auto* col : mColliders)
    {
        if (!col || !col->GetEnabled()) continue;
        if (col == ignore) continue;

        if ((col->GetFlags() & groundMask) == 0) continue;
        if (!col->HasAnyFlag(C_GROUND)) continue; // 念のため

        const Cube aabb = col->GetBoundingVolume()->GetWorldAABB();

        // XZ が AABB 内か？
        const float eps = 0.001f;
        if (samplePos.x < aabb.min.x - eps || samplePos.x > aabb.max.x + eps) continue;
        if (samplePos.z < aabb.min.z - eps || samplePos.z > aabb.max.z + eps) continue;

        const float y = aabb.max.y;

        // startY より上の床は “足元床” としては扱いにくいので除外（天井誤判定対策）
        if (y > startY + mGroundEpsY) continue;

        // 範囲内（下方向 maxDist）
        if (y < yMin) continue;

        if (y > bestY)
        {
            bestY = y;

            bestHit.hit      = true;
            bestHit.y        = y;
            bestHit.distance = std::max(0.0f, startY - y);
            bestHit.pos      = Vector3(samplePos.x, y, samplePos.z);
            bestHit.normal   = Vector3::UnitY;
            bestHit.source   = GroundSource::Collider;
            bestHit.collider = col;
        }
    }

    //==============================
    // (B) Terrain（Polygon）
    //==============================
    {
        GroundHit th;
        Vector3 queryPos(samplePos.x, samplePos.y, samplePos.z);

        if (GetGroundHitAt(queryPos, th))
        {
            const float y = th.y;

            // startY より上は除外（天井/上空ポリゴン誤判定対策）
            if (y <= startY + mGroundEpsY && y >= yMin)
            {
                if (y > bestY)
                {
                    bestY = y;
                    bestHit = th;

                    bestHit.distance = std::max(0.0f, startY - y);
                    bestHit.pos = Vector3(samplePos.x, y, samplePos.z);
                }
            }
        }
    }

    if (!bestHit.hit)
    {
        return false;
    }

    outHit = bestHit;
    return true;
}

//-----------------------------------------------------------------------------
// Actor の FootCollider を使って “斜面＆角落下” 対応の地面ヒットを返す
//  - 5点サンプル
//  - 斜面角がきつい（normal.y が小さい）→床扱いしない
//  - 有効サンプル数が少ない → 角（エッジ）落下扱い
//-----------------------------------------------------------------------------
bool PhysWorld::GetFootGroundHit_Sampled(const Actor* a,
                                        uint32_t groundMask,
                                        GroundHit& outHit) const
{
    outHit = GroundHit{};
    if (!a) return false;

    const ColliderComponent* foot = FindFootCollider(a);
    if (!foot) return false;

    auto obb = foot->GetBoundingVolume()->GetOBB();
    if (!obb) return false;

    // 足下面からちょい上を開始点にして下へ
    const float startY = (obb->pos - obb->axisY * obb->radius.y).y + 0.10f;
    const float maxDist = 1.0f; // いったん固定。必要なら足速度で広げる

    std::vector<Vector3> points;
    BuildFootSamplePoints(foot, points);
    if (points.empty()) return false;

    const float cosMaxSlope = std::cos(DegToRad(mMaxGroundSlopeDeg));

    int validCount = 0;
    float bestY = -std::numeric_limits<float>::max();
    GroundHit bestHit;

    for (const auto& p : points)
    {
        GroundHit h;
        if (!SampleGroundAtPoint(p, startY, maxDist, groundMask, foot, h))
        {
            continue;
        }

        // 斜面判定：normal.y が小さい（急）なら床扱いしない
        // Collider床は UnitY なので必ず通る
        if (h.normal.y < cosMaxSlope)
        {
            continue;
        }

        ++validCount;

        // “一番高い床” を採用（段差で沈まない）
        if (h.y > bestY)
        {
            bestY = h.y;
            bestHit = h;
        }
    }

    // 角（エッジ）落下：支持点が少ないなら床にしない
    if (validCount < mMinSupportSamples)
    {
        return false;
    }

    outHit = bestHit;
    return true;
}

} // namespace toy
