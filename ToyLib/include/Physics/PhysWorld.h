//=============================================================================
// PhysWorld.h
//  - Collider の集約管理と、ゲーム向けの軽量な衝突/地面判定ユーティリティ
//  - OBB(AABB含む) / Sphere / Ray / TerrainPolygons を扱う
//  - 地面判定は「C_GROUND コライダー + TerrainPolygons」のハイブリッド方式
//=============================================================================
#pragma once

#include "Utils/MathUtil.h"                // Vector3 / Ray / OBB / RaycastHit など
#include "Physics/ColliderComponent.h"     // ColliderComponent / Flags
#include <cstdint>
#include <vector>

namespace toy {

//=============================================================================
// MTVResult
//  - OBB vs OBB の SAT で得た最小移動ベクトル（押し戻し）の情報
//=============================================================================
struct MTVResult
{
    Vector3 axis  = Vector3::Zero;        // 押し戻し方向（正規化前の可能性あり）
    float   depth = Math::Infinity;       // めり込み量（小さいほど浅い）
    bool    valid = false;                // 有効フラグ
};

//=============================================================================
// GroundHit / GroundSource
//  - 地面問い合わせの結果（高さだけでなく、法線やヒット位置も返す）
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

    float yGap = Math::Infinity;          // footY - y（呼び出し側のスナップ判定に利用）
    GroundSource source = GroundSource::None;

    const ColliderComponent* collider = nullptr; // source == Collider のときのみ
};


// PhysWorld.h（privateに追加）
struct TerrainGrid
{
    bool enabled = false;

    Vector2 origin = Vector2::Zero; // グリッド原点（minX, minZ）
    float   cellSize = 1.0f;

    int cols = 0;
    int rows = 0;

    // cell -> polygon indices
    std::vector<std::vector<int>> cells;

    void Clear()
    {
        enabled = false;
        origin = Vector2::Zero;
        cellSize = 1.0f;
        cols = rows = 0;
        cells.clear();
    }

    int CellIndex(int cx, int cz) const { return cz * cols + cx; }

    bool IsValidCell(int cx, int cz) const
    {
        return (cx >= 0 && cx < cols && cz >= 0 && cz < rows);
    }
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
    void AddCollider(ColliderComponent* c);
    void RemoveCollider(ColliderComponent* c);

    int GetColliderCount() const { return static_cast<int>(mColliders.size()); }

    //-------------------------------------------------------------------------
    // Ground query
    //-------------------------------------------------------------------------
    // TerrainPolygons のみを対象に、指定XZの地表高さを返す（見つからない場合は -∞）
    float GetGroundHeightAt(const Vector3& pos) const;

    // ハイブリッド地面判定：足元から見て「最も高い地面」を outHit に返す
    bool GetNearestGroundHit(const class Actor* a, GroundHit& outHit) const;

    // 互換用（高さだけ欲しい場合）：GetNearestGroundHit の薄いラッパ
    bool GetNearestGroundY(const Actor* a, float& outY) const;

    // 地形ポリゴン（三角形配列）を登録
    void SetGroundPolygons(const std::vector<struct Polygon>& polys);

    //-------------------------------------------------------------------------
    // Ray utilities
    //-------------------------------------------------------------------------
    // 移動線分（start→end）が壁に当たる場合、ヒット位置（少し手前）を返す
    bool RayHitWall(const Vector3& start,
                    const Vector3& end,
                    Vector3& hitPos) const;

    // Ray vs OBB の交差判定（t を返す）
    bool IntersectRayOBB(const Ray& ray,
                         const struct OBB* obb,
                         float& outT) const;

    // 汎用レイキャスト（flagMask対象）: 最も近いヒットを outHit に返す
    bool Raycast(const Vector3& origin,
                 const Vector3& dir,
                 float maxDist,
                 uint32_t flagMask,
                 RaycastHit& outHit) const;

    //-------------------------------------------------------------------------
    // Collision pair scan
    //-------------------------------------------------------------------------
    // flagA & flagB の組み合わせで衝突ペアを探索し、必要なら押し戻しも行う
    void CollideAndCallback(uint32_t flagA,
                            uint32_t flagB,
                            bool doPushBack = false,
                            bool allowY = false,
                            bool stopVerticalSpeed = false);

private:
    //-------------------------------------------------------------------------
    // OBB / Sphere collision (internal)
    //-------------------------------------------------------------------------
    bool CompareLengthOBB(const struct OBB* cA,
                          const struct OBB* cB,
                          const Vector3& vSep,
                          const Vector3& vDistance);

    bool JudgeWithOBB(ColliderComponent* col1,
                      ColliderComponent* col2);

    bool IsCollideBoxOBB(const OBB* cA,
                         const OBB* cB);

    bool JudgeWithRadius(ColliderComponent* col1,
                         ColliderComponent* col2);

    //-------------------------------------------------------------------------
    // MTV (push back)
    //-------------------------------------------------------------------------
    Vector3 ComputePushBackDirection(ColliderComponent* a,
                                     ColliderComponent* b,
                                     bool allowY);

    bool CompareLengthOBB_MTV(const OBB* cA,
                              const OBB* cB,
                              const Vector3& vSep,
                              const Vector3& vDistance,
                              MTVResult& mtv);

    bool IsCollideBoxOBB_MTV(const OBB* cA,
                             const OBB* cB,
                             MTVResult& mtv);

    //-------------------------------------------------------------------------
    // Terrain polygon utilities (internal)
    //-------------------------------------------------------------------------
    bool IsInPolygon(const struct Polygon* pl,
                     const Vector3& p) const;

    float PolygonHeight(const struct Polygon* pl,
                        const Vector3& p) const;

    Vector3 PolygonNormal(const Polygon& pl) const;

    // pos の真下にある TerrainPolygons の GroundHit（Terrain限定）を返す
    bool GetGroundHitAt(const Vector3& pos, GroundHit& outHit) const;

    // Actor が持つ C_FOOT コライダーを取得（地面判定の基準）
    ColliderComponent* FindFootCollider(const Actor* a) const;

private:
    //-------------------------------------------------------------------------
    // Stored data
    //-------------------------------------------------------------------------
    std::vector<ColliderComponent*> mColliders; // 登録された全コライダー
    std::vector<struct Polygon>     mTerrainPolygons; // 静的地形（三角形配列）
    TerrainGrid mTerrainGrid;

};

} // namespace toy
