//=============================================================================
// PhysWorld.h
//  - Collider の集約管理と、ゲーム向けの軽量な衝突/地面判定ユーティリティ
//  - OBB(AABB含む) / Sphere / Ray / TerrainPolygons を扱う
//  - 地面判定は「C_GROUND コライダー + TerrainPolygons」のハイブリッド方式
//=============================================================================
#pragma once

#include "Utils/MathUtil.h"                // Vector3 / Vector2 / Ray / OBB / RaycastHit など
#include "Physics/ColliderComponent.h"     // ColliderComponent / Flags (C_* など)
#include <cstdint>
#include <vector>

namespace toy {

//=============================================================================
// MTVResult
//  - OBB vs OBB の SAT で得た最小移動ベクトル（押し戻し）の情報
//=============================================================================
struct MTVResult
{
    Vector3 axis  = Vector3::Zero;        // 押し戻し方向（※正規化前の可能性あり）
    float   depth = Math::Infinity;       // めり込み量（小さいほど浅い）
    bool    valid = false;                // 有効フラグ（更新されたら true）
};

//=============================================================================
// GroundHit / GroundSource
//  - 地面問い合わせの結果
//  - 高さだけでなく、法線やヒット位置（代表点）も返す
//=============================================================================
enum class GroundSource : uint8_t
{
    None,
    Terrain,   // TerrainPolygons 由来
    Collider,  // C_GROUND コライダー由来
};

struct GroundHit
{
    bool hit = false;

    float   y      = 0.0f;                // 地面の高さ（足元が乗る想定のY）
    Vector3 pos    = Vector3::Zero;       // ヒット位置（代表点）
    Vector3 normal = Vector3::UnitY;      // 地面法線（上向きを期待）

    // footY - y（足が地面より上なら正）
    // ※呼び出し側（GravityComponentなど）が「スナップして良いか」の判定に使う想定。
    float yGap = Math::Infinity;

    GroundSource source = GroundSource::None;

    // source == Collider のときのみ有効（Terrain のときは nullptr）
    const ColliderComponent* collider = nullptr;
};

//=============================================================================
// PhysWorld
//  - 毎フレームの衝突ペア判定（コールバック登録）
//  - RayCast / RayCCD（壁ヒット）
//  - 地面問い合わせ（GroundHit / GroundY）
//=============================================================================
class PhysWorld
{
public:
    PhysWorld();
    ~PhysWorld();

    //-------------------------------------------------------------------------
    // Main update
    //-------------------------------------------------------------------------
    // 毎フレーム呼び出す衝突処理（コライダーペア走査 + 必要な処理）
    void Test();

    //-------------------------------------------------------------------------
    // Collider management
    //-------------------------------------------------------------------------
    // 物理ワールドに Collider を登録/解除する
    // ※Collider の寿命管理は Actor 側（PhysWorld は生ポインタを保持する）
    void AddCollider(ColliderComponent* c);
    void RemoveCollider(ColliderComponent* c);

    // デバッグ用途：現在登録されている Collider の数
    int GetColliderCount() const { return static_cast<int>(mColliders.size()); }

    //-------------------------------------------------------------------------
    // Ground query
    //-------------------------------------------------------------------------
    // TerrainPolygons のみを対象に、指定XZの地表高さを返す（見つからない場合は -∞）
    float GetGroundHeightAt(const Vector3& pos) const;

    // ハイブリッド地面判定：
    //  - Actor の足元（C_FOOT）を基準に
    //    1) C_GROUND コライダー
    //    2) TerrainPolygons（地形三角形）
    // から「最も高い地面」を outHit に返す
    bool GetNearestGroundHit(const class Actor* a, GroundHit& outHit) const;

    // 互換用（高さだけ欲しい場合）：GetNearestGroundHit の薄いラッパ
    bool GetNearestGroundY(const Actor* a, float& outY) const;

    // 地形ポリゴン（三角形配列）を登録する
    // ※必要ならここでグリッド化などの前処理を行う想定
    void SetGroundPolygons(const std::vector<struct Polygon>& polys);

    //-------------------------------------------------------------------------
    // Ray utilities
    //-------------------------------------------------------------------------
    // 移動線分（start→end）が壁に当たる場合、ヒット位置（少し手前）を返す
    //  - CCD 的に「移動先が壁を貫通しない」ようにする用途
    bool RayHitWall(const Vector3& start,
                    const Vector3& end,
                    Vector3& hitPos) const;

    // Ray vs OBB の交差判定（t を返す）
    //  - outT は ray.start + ray.dir * outT の t 値
    bool IntersectRayOBB(const Ray& ray,
                         const struct OBB* obb,
                         float& outT) const;

    // 汎用レイキャスト（flagMask対象）:
    //  - 最も近いヒットを outHit に返す
    bool Raycast(const Vector3& origin,
                 const Vector3& dir,
                 float maxDist,
                 uint32_t flagMask,
                 RaycastHit& outHit) const;

    //-------------------------------------------------------------------------
    // Collision pair scan
    //-------------------------------------------------------------------------
    // flagA & flagB の組み合わせで衝突ペアを探索し、必要なら押し戻しも行う
    //  - doPushBack        : MTV による押し戻し（位置補正）を行う
    //  - allowY            : Y成分も押し戻す（通常は壁ずりのため false）
    //  - stopVerticalSpeed : 垂直速度を止める（床に着地した瞬間など）
    void CollideAndCallback(uint32_t flagA,
                            uint32_t flagB,
                            bool doPushBack = false,
                            bool allowY = false,
                            bool stopVerticalSpeed = false);

    // 上向き移動中の天井ヒットを解決する（押し戻しベクトルを返す）
    //  - moverFlag   : 対象（例: C_PLAYER / C_ENEMY）
    //  - ceilingFlag : 天井側（例: C_CEILING）
    //  - outPush     : 押し戻し量（通常は下方向が入る）
    bool ResolveCeiling(class Actor* a,
                        uint32_t moverFlag,
                        uint32_t ceilingFlag,
                        Vector3& outPush) const;
    
private:
    //-------------------------------------------------------------------------
    // OBB / Sphere collision (internal)
    //-------------------------------------------------------------------------
    // SAT：分離軸 vSep 上に投影して「分離しているか」を判定
    bool CompareLengthOBB(const struct OBB* cA,
                          const struct OBB* cB,
                          const Vector3& vSep,
                          const Vector3& vDistance) const;

    // Collider から OBB を取り出して SAT 判定
    bool JudgeWithOBB(const ColliderComponent* col1,
                      const ColliderComponent* col2) const;

    // OBB vs OBB（SAT）
    bool IsCollideBoxOBB(const OBB* cA,
                         const OBB* cB) const;

    // BoundingSphere で早期判定（軽量）
    bool JudgeWithRadius(const ColliderComponent* col1,
                         const ColliderComponent* col2) const;

    //-------------------------------------------------------------------------
    // MTV (push back)
    //-------------------------------------------------------------------------
    // 2 Collider の MTV を求めて押し戻しベクトルを返す
    //  - allowY=false のときは Y を 0 にして「壁ずり」しやすくする
    Vector3 ComputePushBackDirection(const ColliderComponent* a,
                                     const ColliderComponent* b,
                                     bool allowY) const;

    // SAT + MTV：最小めり込み軸を mtv に記録
    bool CompareLengthOBB_MTV(const OBB* cA,
                              const OBB* cB,
                              const Vector3& vSep,
                              const Vector3& vDistance,
                              MTVResult& mtv) const;

    bool IsCollideBoxOBB_MTV(const OBB* cA,
                             const OBB* cB,
                             MTVResult& mtv) const;

    //-------------------------------------------------------------------------
    // Terrain polygon utilities (internal)
    //-------------------------------------------------------------------------
    // XZ 平面上で「点が三角形内にあるか」判定
    bool IsInPolygon(const struct Polygon* pl,
                     const Vector3& p) const;

    // ポリゴン上の高さ（Y）を返す（XZ投影点 p に対して平面方程式で求める）
    float PolygonHeight(const struct Polygon* pl,
                        const Vector3& p) const;

    // ポリゴンの法線（正規化）を返す。計算不能なら UnitY。
    // ※Up方向（UnitY）と Dot して負なら反転して「上向き」を保証する想定。
    Vector3 PolygonNormal(const Polygon& pl) const;

    // pos の真下にある TerrainPolygons の GroundHit（Terrain限定）を返す
    bool GetGroundHitAt(const Vector3& pos, GroundHit& outHit) const;

    // Actor が持つ C_FOOT コライダーを取得（地面判定の基準）
    ColliderComponent* FindFootCollider(const Actor* a) const;

private:
    //-------------------------------------------------------------------------
    // Terrain grid (optional acceleration structure)
    //-------------------------------------------------------------------------
    // TerrainPolygons を 2D グリッドに割り当てて、
    // GetGroundHitAt() の走査対象（三角形）を絞るための補助構造。
    //
    // ※enabled=false の場合は「全走査」フォールバックで動かせる。
    struct TerrainGrid
    {
        bool enabled = false;

        Vector2 origin  = Vector2::Zero; // グリッド原点（minX, minZ）
        float   cellSize = 1.0f;

        int cols = 0;
        int rows = 0;

        // cell -> polygon indices
        //  - CellIndex(cx,cz) で 1次元化し、cells[idx] にポリゴンID配列を持つ
        std::vector<std::vector<int>> cells;

        void Clear()
        {
            enabled  = false;
            origin   = Vector2::Zero;
            cellSize = 1.0f;
            cols = 0;
            rows = 0;
            cells.clear();
        }

        int CellIndex(int cx, int cz) const { return cz * cols + cx; }

        bool IsValidCell(int cx, int cz) const
        {
            return (cx >= 0 && cx < cols && cz >= 0 && cz < rows);
        }
    };

private:
    //-------------------------------------------------------------------------
    // Stored data
    //-------------------------------------------------------------------------
    // 登録された全コライダー（寿命管理は外部）
    std::vector<ColliderComponent*> mColliders;

    // 静的地形（三角形配列）
    std::vector<struct Polygon> mTerrainPolygons;

    // TerrainPolygons の加速構造（任意）
    TerrainGrid mTerrainGrid;
};

} // namespace toy


// memo:
//  - 物理/幾何ユーティリティ化候補
//      IntersectRayOBB
//      IntersectRayTriangle（※現状は別所にある想定）
//      IsInPolygon / PolygonHeight / PolygonNormal
//    などを toy::physmath 的に分離するのはアリ。
//  - ただし ToyLib の方針（読みやすさ優先）なら、PhysWorld に置いたままでもOK。
