//=============================================================================
// PhysWorld.h
//=============================================================================
#pragma once

#include "Utils/MathUtil.h"
#include "Physics/ColliderFlags.h"
#include "Asset/Geometry/Polygon.h"

#include <cfloat>
#include <cstdint>
#include <vector>

namespace toy {

//=============================================================================
// 地面ヒット情報
//=============================================================================
enum class GroundSource
{
    None,
    Collider, // C_GROUND を持つ Collider（床）のヒット
    Terrain   // 地形ポリゴン（Polygon）から算出した高さ
};

// GroundHit:
//  - y        : ヒットした地面の高さ
//  - distance : startY から地面までの距離（下方向）。すでに潜っている場合は 0
//  - pos      : (x, y, z) のヒット位置
//  - normal   : 地形なら法線。Collider床は基本 UnitY（または床面法線）
struct GroundHit
{
    bool hit { false };
    
    float y        { -FLT_MAX };
    float distance {  FLT_MAX };
    
    Vector3 pos    { Vector3::Zero };
    Vector3 normal { Vector3::UnitY };
    
    GroundSource source { GroundSource::None };
    const class ColliderComponent* collider { nullptr };
};

//=============================================================================
// Ray / Raycast
//=============================================================================
struct Ray
{
    Vector3 start { Vector3::Zero };
    Vector3 dir   { Vector3::UnitZ }; // 正規化前提（コンストラクタで正規化する）

    Ray() = default;

    Ray(const Vector3& s, const Vector3& d)
        : start(s)
        , dir(d)
    {
        if (dir.LengthSq() > Math::NearZeroEpsilon)
        {
            dir.Normalize();
        }
        else
        {
            dir = Vector3::UnitZ;
        }
    }
};

struct RaycastHit
{
    bool hit { false };

    Vector3 point    { Vector3::Zero };  // ヒット位置（ワールド）
    Vector3 normal   { Vector3::UnitY }; // ヒット面法線（ワールド・正規化）
    float   distance { 0.0f };

    class Actor* actor { nullptr };
};

// Ray vs triangle (Möller–Trumbore)
bool IntersectRayTriangle(const Ray& ray,
                          const Vector3& v0,
                          const Vector3& v1,
                          const Vector3& v2,
                          float& outT);

//=============================================================================
// 視界クエリ（センサー用途）
//=============================================================================
struct ViewQueryDesc
{
    Vector3 origin  { Vector3::Zero };
    Vector3 forward { Vector3::UnitZ }; // 内部で正規化
    
    float fovRad  { Math::ToRadians(90.0f) };
    float maxDist { 30.0f };

    // 候補条件（Any）:
    //  (colliderFlags & flagMask) != 0 のものを候補にする
    uint32_t flagMask {};

    // 遮蔽（LOS）
    bool         requireLOS   { true };
    uint32_t     losBlockMask { 0 };        // 壁など遮蔽物のマスク
    class Actor* ignoreActor  { nullptr };  // 自分など（遮蔽判定でも除外）

    // 近接オーバーライド
    float nearOverrideDist { 2.0f };
    bool  nearOverrideRequireLOS { false };
};

struct ViewQueryHit
{
    class Actor*             actor    { nullptr };
    class ColliderComponent* collider { nullptr };
    
    float dist     { 0.0f };
    float cosAngle { -1.0f };
};

//=============================================================================
// 地形グリッド（任意：地形探索の高速化）
//=============================================================================
struct TerrainGrid
{
    bool enabled { false };

    // origin.x = minX, origin.y = minZ
    Vector2 origin   { Vector2::Zero };
    float   cellSize { 10.0f };

    int cols {};
    int rows {};

    // 各セルに「地形ポリゴンのインデックス」一覧を持つ
    std::vector<std::vector<int>> cells;

    void Clear()
    {
        enabled  = false;
        origin   = Vector2::Zero;
        cellSize = 10.0f;
        cols = 0;
        rows = 0;
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

//=============================================================================
// PhysWorld
//=============================================================================
class PhysWorld
{
public:
    PhysWorld() = default;
    
    //-------------------------------------------------------------------------
    // Terrain
    //-------------------------------------------------------------------------
    void  SetGroundPolygons(const std::vector<Polygon>& polys);
    float GetGroundHeightAt(const Vector3& pos) const;
    
    // Terrain only
    bool GetGroundHitAt(const Vector3& pos, GroundHit& outHit) const;
    
    //-------------------------------------------------------------------------
    // Ground query (Collider + Terrain)
    //-------------------------------------------------------------------------
    bool GetNearestGroundY(const Actor* a, float& outY) const;
    bool GetNearestGroundHit(const Actor* a, GroundHit& outHit) const;
    
    bool GetNearestGroundHitAtXZ(const Vector3& pos, GroundHit& outHit) const;
    
    bool GetGroundHitRayDown(const Vector3& origin,
                             float maxDist,
                             uint32_t groundMask,
                             GroundHit& outHit) const;
    
    bool GetGroundHitSweepDown(float startY,
                               float endY,
                               float x,
                               float z,
                               uint32_t groundMask,
                               GroundHit& outHit,
                               const ColliderComponent* ignore) const;
    
    //=============================================================
    // Foot ground sampling (NEW)  ※Gravity から呼ぶ
    //=============================================================
    bool GetFootGroundHit_Sampled(const Actor* a,
                                  uint32_t groundMask,
                                  GroundHit& outHit) const;
    
    //-------------------------------------------------------------------------
    // Ray
    //-------------------------------------------------------------------------
    bool RayHitWall(const Vector3& start, const Vector3& end, Vector3& hitPos) const;
    bool RayHitWallEx(const Vector3& start,
                      const Vector3& end,
                      uint32_t wallMask,
                      const Actor* ignoreActor,
                      float cosFloorLike,          // 例: cos(45deg)=0.707
                      Vector3& outHitPos,
                      Vector3& outHitNormal) const;
    
    
    bool Raycast(const Vector3& origin,
                 const Vector3& dir,
                 float maxDist,
                 uint32_t flagMask,
                 RaycastHit& outHit) const;
    
    //-------------------------------------------------------------------------
    // View query
    //-------------------------------------------------------------------------
    void QueryView(const ViewQueryDesc& desc,
                   std::vector<ViewQueryHit>& outHits) const;
    
    //-------------------------------------------------------------------------
    // Collider list management
    //-------------------------------------------------------------------------
    void AddCollider(ColliderComponent* c);
    void RemoveCollider(ColliderComponent* c);
    
    void GetCollidersByAnyFlags(uint32_t mask,
                                std::vector<ColliderComponent*>& out) const;
    
    void GetCollidersByAllFlags(uint32_t mask,
                                std::vector<ColliderComponent*>& out) const;
    
    //-------------------------------------------------------------------------
    // Pair test / callback
    //-------------------------------------------------------------------------
    void CollideAndCallback(uint32_t flagA,
                            uint32_t flagB,
                            bool doPushBack = false,
                            bool allowY = false,
                            bool stopVerticalSpeed = false
                            );
    
    void Test();
    
    //-------------------------------------------------------------------------
    // Utility
    //-------------------------------------------------------------------------
    Vector3 ComputePushBackDirection(const ColliderComponent* a,
                                     const ColliderComponent* b,
                                     bool allowY) const;
    
    bool ResolveCeiling(Actor* a,
                        uint32_t moverFlag,
                        uint32_t ceilingFlag,
                        Vector3& outPush) const;
    
    size_t GetColliderCount() const { return mColliders.size(); }
    

    //=============================================================
    // Collider validation (dangling pointer prevention)
    //=============================================================
    bool HasCollider(const ColliderComponent* c) const;


private:
    //-------------------------------------------------------------------------
    // Terrain helpers
    //-------------------------------------------------------------------------
    bool    IsInPolygon(const Polygon* pl, const Vector3& p) const;
    float   PolygonHeight(const Polygon* pl, const Vector3& p) const;
    Vector3 PolygonNormal(const Polygon& pl) const;
    
    ColliderComponent* FindFootCollider(const Actor* a) const;
    
    //-------------------------------------------------------------------------
    // OBB / Sphere
    //-------------------------------------------------------------------------
    bool JudgeWithRadius(const ColliderComponent* col1,
                         const ColliderComponent* col2) const;
    
    bool JudgeWithOBB(const ColliderComponent* col1,
                      const ColliderComponent* col2) const;
    
    bool CompareLengthOBB(const struct OBB* cA,
                          const struct OBB* cB,
                          const Vector3& vSep,
                          const Vector3& vDistance) const;
    
    bool IsCollideBoxOBB(const OBB* cA, const OBB* cB) const;
    
    //-------------------------------------------------------------------------
    // MTV
    //-------------------------------------------------------------------------
    struct MTVResult
    {
        bool    valid = false;
        float   depth = Math::Infinity;
        Vector3 axis  = Vector3::Zero; // normalized
    };
    
    bool CompareLengthOBB_MTV(const struct OBB* cA,
                              const struct OBB* cB,
                              const Vector3& vSep,
                              const Vector3& vDistance,
                              MTVResult& mtv) const;
    
    bool IsCollideBoxOBB_MTV(const struct OBB* cA,
                             const struct OBB* cB,
                             MTVResult& mtv) const;
    
    //-------------------------------------------------------------------------
    // OBB contact / Ray
    //-------------------------------------------------------------------------
    bool IntersectRayOBB(const Ray& ray,
                         const struct OBB* obb,
                         float& outT) const;
    
    struct Contact
    {
        bool hit { false };
        
        float   depth  { 0.0f };
        Vector3 normal { Vector3::UnitY }; // direction to push A
        Vector3 mtv    { Vector3::Zero };  // normal * (depth + eps)
    };
    
    bool IntersectOBBContact(const OBB* a, const OBB* b, Contact& out) const;
    
    bool IntersectRayOBB_WithNormal(const Ray& ray,
                                    const struct OBB* obb,
                                    float& outT,
                                    Vector3& outNormal) const;
    
    //=============================================================
    // Foot sampling helpers (NEW)
    //=============================================================
    bool SampleGroundAtPoint(const Vector3& samplePos,   // xz is important
                             float startY,
                             float maxDist,
                             uint32_t groundMask,
                             const ColliderComponent* ignore,
                             GroundHit& outHit) const;

    void BuildFootSamplePoints(const ColliderComponent* foot,
                               std::vector<Vector3>& outPoints) const;

    bool TryGetColliderTopHitAtXZ(const ColliderComponent* c,
                                  float x,
                                  float z,
                                  float startY,
                                  float endY,
                                  float cosMaxSlope,
                                  GroundHit& outHit) const;

private:
    std::vector<Polygon> mTerrainPolygons;
    TerrainGrid          mTerrainGrid{};
    
    std::vector<ColliderComponent*> mColliders;
    
    //=============================================================
    // Tunables (NEW)
    //=============================================================
    float mMaxGroundSlopeDeg { 45.0f }; // steeper than this is not "ground"
    int   mMinSupportSamples { 3 };     // out of 5
    float mFootSampleInset   { 0.03f }; // shrink sample from edge
    float mGroundEpsY        { 0.02f }; // tolerance
};

} // namespace toy
