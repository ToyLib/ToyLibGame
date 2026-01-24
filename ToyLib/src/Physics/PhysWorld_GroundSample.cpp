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

//-----------------------------------------------------------------------------
// Foot sample points: OBB姿勢を考慮した「下面5点」（＋型）
//-----------------------------------------------------------------------------
void PhysWorld::BuildFootSamplePoints(const ColliderComponent* foot,
                                      std::vector<Vector3>& outPoints) const
{
    outPoints.clear();
    if (!foot) return;

    auto obbSP = foot->GetBoundingVolume()->GetOBB();
    if (!obbSP) return;

    const OBB& obb = *obbSP;

    // 軸は必ず正規化（入ってないと距離が狂う）
    Vector3 ax = obb.axisX;
    Vector3 ay = obb.axisY;
    Vector3 az = obb.axisZ;

    if (ax.LengthSq() > Math::NearZeroEpsilon) ax.Normalize(); else ax = Vector3::UnitX;
    if (ay.LengthSq() > Math::NearZeroEpsilon) ay.Normalize(); else ay = Vector3::UnitY;
    if (az.LengthSq() > Math::NearZeroEpsilon) az.Normalize(); else az = Vector3::UnitZ;

    // サンプルの“基準XZ”は bottomCenter じゃなくて OBB中心XZ を使う
    // Y は Sample 側で startY を与えるので、ここでは 0 でOK
    const Vector3 base(obb.pos.x, 0.0f, obb.pos.z);

    const float rx = Math::Max(0.0f, obb.radius.x - mFootSampleInset);
    const float rz = Math::Max(0.0f, obb.radius.z - mFootSampleInset);

    // + 型：中心、左右、前後
    outPoints.push_back(base);            // center
    outPoints.push_back(base + ax * rx);  // +X
    outPoints.push_back(base - ax * rx);  // -X
    outPoints.push_back(base + az * rz);  // +Z
    outPoints.push_back(base - az * rz);  // -Z
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

    if (maxDist <= 0.0f)
    {
        return false;
    }

    const float endY = startY - maxDist;

    bool  found = false;
    float bestY = -FLT_MAX;
    GroundHit bestHit;

    const float cosMaxSlope = cosf(Math::ToRadians(mMaxGroundSlopeDeg));

    //==============================
    // (A) Collider床：OBB上面サンプル（統一）
    //==============================
    for (const ColliderComponent* col : mColliders)
    {
        if (!col || !col->GetEnabled()) continue;
        if (col == ignore) continue;

        if (!col->HasFlag(groundMask)) continue; // groundMask 条件
        if (!col->HasFlag(C_GROUND))   continue; // 念のため（床として扱うもの）

        GroundHit h;
        if (!TryGetColliderTopHitAtXZ(col,
                                      samplePos.x,
                                      samplePos.z,
                                      startY,
                                      endY,
                                      cosMaxSlope,
                                      h))
        {
            continue;
        }

        // “最も高い床” を採用（段差で沈まない）
        if (!found || h.y > bestY)
        {
            found   = true;
            bestY   = h.y;
            bestHit = h;
        }
    }

    //==============================
    // (B) Terrain（既存のまま）
    //==============================
    {
        GroundHit th;
        // GetGroundHitAt は XZ のみ見てるはずなので、Y は startY を入れておく
        Vector3 query(samplePos.x, startY, samplePos.z);

        if (GetGroundHitAt(query, th))
        {
            if (th.y <= startY + mGroundEpsY && th.y >= endY)
            {
                if (!found || th.y > bestY)
                {
                    found   = true;
                    bestY   = th.y;
                    bestHit = th;

                    // 念のため pos を samplePos のXZに揃える
                    bestHit.pos.x = samplePos.x;
                    bestHit.pos.z = samplePos.z;

                    bestHit.distance = std::max(0.0f, startY - th.y);
                }
            }
        }
    }

    if (!found)
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

    auto obbSP = foot->GetBoundingVolume()->GetOBB();
    if (!obbSP) return false;

    const OBB& obb = *obbSP;

    // ------------------------------------------------------------
    // 1) startY / maxDist を足サイズ基準で安全側に
    //    - bottom から 시작すると坂の上側を取りこぼす
    // ------------------------------------------------------------
    Vector3 ay = obb.axisY;
    if (ay.LengthSq() > Math::NearZeroEpsilon) ay.Normalize();
    else ay = Vector3::UnitY;

    // 足の上面中心からスタート（坂の上側を確実に拾う）
    const float startY = (obb.pos + ay * obb.radius.y).y + 0.10f;

    // 足の高さ(2*radius.y) + 余裕ぶん。短すぎる 1.0f をやめる
    const float maxDist = (obb.radius.y * 2.0f) + 1.0f;

    std::vector<Vector3> points;
    BuildFootSamplePoints(foot, points);
    if (points.empty()) return false;

    // ------------------------------------------------------------
    // 2) スロープ判定
    // ------------------------------------------------------------
    const float cosMaxSlope = std::cos(Math::ToRadians(mMaxGroundSlopeDeg));

    // 有効ヒットを集める
    struct Sample
    {
        GroundHit hit;
        bool isCenter = false;
    };
    std::vector<Sample> samples;
    samples.reserve(points.size());

    for (size_t i = 0; i < points.size(); ++i)
    {
        const Vector3& p = points[i];

        GroundHit h;
        if (!SampleGroundAtPoint(p, startY, maxDist, groundMask, foot, h))
        {
            continue;
        }

        // 急すぎる面は床扱いしない（Terrain / OBBどちらも normal が入ってる前提）
        if (h.normal.y < cosMaxSlope)
        {
            continue;
        }

        Sample s;
        s.hit      = h;
        s.isCenter = (i == 0); // ★BuildFootSamplePoints で中心を先頭にしておく
        samples.push_back(s);
    }

    // ------------------------------------------------------------
    // 3) 角落下（支持点不足）
    // ------------------------------------------------------------
    if ((int)samples.size() < mMinSupportSamples)
    {
        return false;
    }

    // ------------------------------------------------------------
    // 4) 出力の作り方：スムーズ優先
    //    - center が取れてるならそれを採用（RayDownに近い挙動）
    //    - 取れてないなら y の中央値を採用（bestYより跳ねにくい）
    // ------------------------------------------------------------
    for (const auto& s : samples)
    {
        if (s.isCenter)
        {
            outHit = s.hit;
            return true;
        }
    }

    // center がない場合：中央値
    std::vector<float> ys;
    ys.reserve(samples.size());
    for (const auto& s : samples) ys.push_back(s.hit.y);

    std::sort(ys.begin(), ys.end());
    const float medianY = ys[ys.size() / 2];

    // medianY に一番近いサンプルを返す（normal / collider を保つ）
    float bestAbs = FLT_MAX;
    GroundHit best{};
    for (const auto& s : samples)
    {
        const float d = std::fabs(s.hit.y - medianY);
        if (d < bestAbs)
        {
            bestAbs = d;
            best    = s.hit;
        }
    }

    outHit = best;
    return true;
}

} // namespace toy
