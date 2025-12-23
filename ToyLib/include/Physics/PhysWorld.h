//=============================================================================
// PhysWorld.h
//  - Collider の集約管理と、ゲーム向けの軽量な衝突/地面判定ユーティリティ
//  - OBB(AABB含む) / Sphere / Ray / TerrainPolygons を扱う
//  - 地面判定は「C_GROUND コライダー + TerrainPolygons」のハイブリッド方式
//=============================================================================

//=============================================================================
// PhysWorld.h
//=============================================================================
#pragma once

#include "Utils/MathUtil.h"
#include "Physics/ColliderFlags.h"
#include "Asset/Geometry/Polygon.h"

#include <vector>
#include <cstdint>

namespace toy {

class Actor;
class ColliderComponent;

//=============================================================================
// Ground hit
//=============================================================================
enum class GroundSource
{
    None,
    Collider,
    Terrain
};

struct GroundHit
{
    bool hit = false;
    float y = 0.0f;
    float yGap = 0.0f;
    Vector3 pos = Vector3::Zero;
    Vector3 normal = Vector3::UnitY;
    GroundSource source = GroundSource::None;
    const ColliderComponent* collider = nullptr;
};

//=============================================================================
// View query (sensor)
//=============================================================================
struct ViewQueryDesc
{
    Vector3   origin       = Vector3::Zero;
    Vector3   forward      = Vector3::UnitZ; // normalized inside
    float     fovRad       = Math::ToRadians(90.0f);
    float     maxDist      = 30.0f;

    // ★ Any 判定： (colliderFlags & flagMask) != 0 で候補
    uint32_t  flagMask     = 0;

    // LOS
    bool      requireLOS   = true;
    uint32_t  losBlockMask = 0;      // 壁など
    Actor*    ignoreActor  = nullptr;
};

struct ViewQueryHit
{
    Actor*            actor    = nullptr;
    ColliderComponent* collider = nullptr;
    float             dist     = 0.0f;
    float             cosAngle = -1.0f;
};

//=============================================================================
// Terrain grid (optional acceleration)
//=============================================================================
struct TerrainGrid
{
    bool enabled = false;

    // origin.x = minX, origin.y = minZ
    Vector2 origin = Vector2::Zero;
    float cellSize = 10.0f;

    int cols = 0;
    int rows = 0;

    std::vector<std::vector<int>> cells;

    void Clear()
    {
        enabled = false;
        origin = Vector2::Zero;
        cellSize = 10.0f;
        cols = rows = 0;
        cells.clear();
    }

    bool IsValidCell(int cx, int cz) const
    {
        return (cx >= 0 && cx < cols && cz >= 0 && cz < rows);
    }

    int CellIndex(int cx, int cz) const
    {
        return cz * cols + cx;
    }
};

//==============================
// Ray（レイ）
//==============================
struct Ray
{
    Vector3 start;  // 始点（ワールド座標）
    Vector3 dir;    // 正規化された方向

    Ray() {}
    Ray(const Vector3& s, const Vector3& d)
        : start(s), dir(Vector3::Normalize(d)) {}
};

//==============================
// RaycastHit（レイ判定結果）
//==============================
struct RaycastHit
{
    bool hit = false;
    Vector3 point = Vector3::Zero;
    float distance = 0.0f;
    class Actor* actor = nullptr;
};

//=============================================================================
// PhysWorld
//=============================================================================
class PhysWorld
{
public:
    PhysWorld();
    ~PhysWorld();

    //------------------------------------------------------------------------
    // Terrain
    //------------------------------------------------------------------------
    void SetGroundPolygons(const std::vector<struct Polygon>& polys);
    float GetGroundHeightAt(const Vector3& pos) const;

    bool GetGroundHitAt(const Vector3& pos, GroundHit& outHit) const;

    bool GetNearestGroundY(const Actor* a, float& outY) const;
    bool GetNearestGroundHit(const Actor* a, GroundHit& outHit) const;

    //------------------------------------------------------------------------
    // Ray
    //------------------------------------------------------------------------
    bool RayHitWall(const Vector3& start, const Vector3& end, Vector3& hitPos) const;

    bool Raycast(const Vector3& origin,
                 const Vector3& dir,
                 float maxDist,
                 uint32_t flagMask,
                 RaycastHit& outHit) const;

    //------------------------------------------------------------------------
    // View query (sensor / lockon candidate)
    //------------------------------------------------------------------------
    void QueryView(const ViewQueryDesc& desc,
                   std::vector<ViewQueryHit>& outHits) const;

    //------------------------------------------------------------------------
    // Collider management
    //------------------------------------------------------------------------
    void AddCollider(ColliderComponent* c);
    void RemoveCollider(ColliderComponent* c);

    // Any / All で明確に分ける
    void GetCollidersByAnyFlags(uint32_t mask,
                                std::vector<ColliderComponent*>& out) const;

    void GetCollidersByAllFlags(uint32_t mask,
                                std::vector<ColliderComponent*>& out) const;

    //------------------------------------------------------------------------
    // Pair scan / callbacks
    //------------------------------------------------------------------------
    void CollideAndCallback(uint32_t flagA,
                            uint32_t flagB,
                            bool doPushBack = false,
                            bool allowY = false,
                            bool stopVerticalSpeed = false);

    void Test();

    //------------------------------------------------------------------------
    // Utilities
    //------------------------------------------------------------------------
    Vector3 ComputePushBackDirection(const ColliderComponent* a,
                                     const ColliderComponent* b,
                                     bool allowY) const;

    bool ResolveCeiling(Actor* a,
                        uint32_t moverFlag,
                        uint32_t ceilingFlag,
                        Vector3& outPush) const;


    // collider count (debug / test / sensor)
    size_t GetColliderCount() const { return mColliders.size(); }

private:
    // Terrain helpers
    bool IsInPolygon(const Polygon* pl, const Vector3& p) const;
    float PolygonHeight(const Polygon* pl, const Vector3& p) const;
    Vector3 PolygonNormal(const Polygon& pl) const;

    // Ground helper
    ColliderComponent* FindFootCollider(const Actor* a) const;

    // OBB / Sphere
    bool JudgeWithRadius(const ColliderComponent* col1,
                         const ColliderComponent* col2) const;

    bool JudgeWithOBB(const ColliderComponent* col1,
                      const ColliderComponent* col2) const;

    // SAT
    bool CompareLengthOBB(const struct OBB* cA,
                          const struct OBB* cB,
                          const Vector3& vSep,
                          const Vector3& vDistance) const;

    bool IsCollideBoxOBB(const OBB* cA, const OBB* cB) const;

    // MTV
    struct MTVResult
    {
        bool valid = false;
        float depth = Math::Infinity;
        Vector3 axis = Vector3::Zero; // normalized
    };

    bool CompareLengthOBB_MTV(const struct OBB* cA,
                              const struct OBB* cB,
                              const Vector3& vSep,
                              const Vector3& vDistance,
                              MTVResult& mtv) const;

    bool IsCollideBoxOBB_MTV(const struct OBB* cA,
                             const struct OBB* cB,
                             MTVResult& mtv) const;

    // Ray vs OBB
    bool IntersectRayOBB(const Ray& ray, const struct OBB* obb, float& outT) const;
    

private:
    // Terrain
    std::vector<Polygon> mTerrainPolygons;
    TerrainGrid          mTerrainGrid;

    // Colliders (raw ptr; owned by Actor via unique_ptr components)
    std::vector<ColliderComponent*> mColliders;
};

} // namespace toy
