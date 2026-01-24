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
    Collider, // C_GROUND を持つ Collider の AABB 上面（max.y）
    Terrain   // 地形ポリゴン（Polygon）から算出した高さ
};

// GroundHit:
//  - y        : ヒットした地面の高さ
//  - distance : origin から地面までの距離（下方向）。すでに潜っている場合は 0
//  - pos      : (x, y, z) のヒット位置
//  - normal   : 地形なら法線。Collider 床は基本 UnitY
struct GroundHit
{
    bool hit = false;

    float y        = -FLT_MAX;
    float distance =  FLT_MAX;

    Vector3 pos    = Vector3::Zero;
    Vector3 normal = Vector3::UnitY;

    GroundSource source = GroundSource::None;
    const class ColliderComponent* collider = nullptr;
};

//=============================================================================
// Ray / Raycast
//=============================================================================
struct Ray
{
    Vector3 start = Vector3::Zero;
    Vector3 dir   = Vector3::UnitZ; // 正規化前提（コンストラクタで正規化する）

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
    bool hit = false;

    Vector3 point = Vector3::Zero;
    float   distance = 0.0f;

    class Actor* actor = nullptr;
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
    Vector3 origin  = Vector3::Zero;
    Vector3 forward = Vector3::UnitZ; // 内部で正規化

    float fovRad  = Math::ToRadians(90.0f);
    float maxDist = 30.0f;

    // 候補条件（Any）:
    //  (colliderFlags & flagMask) != 0 のものを候補にする
    uint32_t flagMask = 0;

    // 遮蔽（LOS）
    bool         requireLOS   = true;
    uint32_t     losBlockMask = 0;           // 壁など遮蔽物のマスク
    class Actor* ignoreActor  = nullptr;     // 自分など（遮蔽判定でも除外）

    // 近接オーバーライド：
    // 近いターゲットは遮蔽を無視したい…などの運用向け
    float nearOverrideDist = 2.0f;
    bool  nearOverrideRequireLOS = false;    // 近くても遮蔽を見たいなら true
};

struct ViewQueryHit
{
    class Actor*             actor    = nullptr;
    class ColliderComponent* collider = nullptr;

    float dist     = 0.0f;
    float cosAngle = -1.0f;
};

//=============================================================================
// 地形グリッド（任意：地形探索の高速化）
//=============================================================================
struct TerrainGrid
{
    bool enabled = false;

    // origin.x = minX, origin.y = minZ
    Vector2 origin   = Vector2::Zero;
    float   cellSize = 10.0f;

    int cols = 0;
    int rows = 0;

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
// 物理・衝突・地面問い合わせなどをまとめた「ワールド」
//=============================================================================
class PhysWorld
{
public:
    PhysWorld();
    ~PhysWorld();

    //-------------------------------------------------------------------------
    // 地形
    //-------------------------------------------------------------------------
    void  SetGroundPolygons(const std::vector<Polygon>& polys);
    float GetGroundHeightAt(const Vector3& pos) const;

    // 地形のみ（Terrain）の高さ・法線を取得
    bool GetGroundHitAt(const Vector3& pos, GroundHit& outHit) const;

    //-------------------------------------------------------------------------
    // 地面問い合わせ（Collider床 + Terrain の合成）
    //-------------------------------------------------------------------------
    bool GetNearestGroundY(const Actor* a, float& outY) const;
    bool GetNearestGroundHit(const Actor* a, GroundHit& outHit) const;

    // 任意の XZ に対して「最も高い地面」を返す（足Collider不要）
    //  - Collider床（C_GROUND）と Terrain の両方から判定
    //  - outHit.pos は (pos.x, y, pos.z) になる
    bool GetNearestGroundHitAtXZ(const Vector3& pos, GroundHit& outHit) const;

    // 下向きレイ（というより縦のスイープ）で「一番近い床」を取る
    //  - Collider床の AABB 上面 or Terrain 高さを候補にする
    //  - 潜り救済は「浅いめり込み」だけ許可（実装側で制御）
    bool GetGroundHitRayDown(const Vector3& origin,
                             float maxDist,
                             uint32_t groundMask,
                             GroundHit& outHit) const;

    // y 区間 [startY -> endY] を下方向にスイープして床を探す
    //  - xz は固定（垂直線分）
    //  - ignore は除外したい Collider（例：自分の床扱いを避けたい場合）
    bool GetGroundHitSweepDown(float startY,
                               float endY,
                               float x,
                               float z,
                               uint32_t groundMask,
                               GroundHit& outHit,
                               const ColliderComponent* ignore) const;

    //-------------------------------------------------------------------------
    // Ray
    //-------------------------------------------------------------------------
    bool RayHitWall(const Vector3& start, const Vector3& end, Vector3& hitPos) const;

    bool Raycast(const Vector3& origin,
                 const Vector3& dir,
                 float maxDist,
                 uint32_t flagMask,
                 RaycastHit& outHit) const;

    //-------------------------------------------------------------------------
    // 視界クエリ
    //-------------------------------------------------------------------------
    void QueryView(const ViewQueryDesc& desc,
                   std::vector<ViewQueryHit>& outHits) const;

    //-------------------------------------------------------------------------
    // Collider 管理（Actor が所有、PhysWorld は参照リストを持つだけ）
    //-------------------------------------------------------------------------
    void AddCollider(ColliderComponent* c);
    void RemoveCollider(ColliderComponent* c);

    void GetCollidersByAnyFlags(uint32_t mask,
                               std::vector<ColliderComponent*>& out) const;

    void GetCollidersByAllFlags(uint32_t mask,
                               std::vector<ColliderComponent*>& out) const;

    //-------------------------------------------------------------------------
    // ペア走査 / コールバック（衝突通知＋必要なら押し戻し）
    //-------------------------------------------------------------------------
    void CollideAndCallback(uint32_t flagA,
                            uint32_t flagB,
                            bool doPushBack = false,
                            bool allowY = false,
                            bool stopVerticalSpeed = false);

    void Test();

    //-------------------------------------------------------------------------
    // ユーティリティ
    //-------------------------------------------------------------------------
    Vector3 ComputePushBackDirection(const ColliderComponent* a,
                                     const ColliderComponent* b,
                                     bool allowY) const;

    // 天井（ceilingFlag）とのめり込み解消（押し戻しベクトルを返す）
    bool ResolveCeiling(Actor* a,
                        uint32_t moverFlag,
                        uint32_t ceilingFlag,
                        Vector3& outPush) const;

    // Debug
    size_t GetColliderCount() const { return mColliders.size(); }

private:
    //-------------------------------------------------------------------------
    // 地形ヘルパー
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
    // MTV（最小移動ベクトル）計算
    //-------------------------------------------------------------------------
    struct MTVResult
    {
        bool    valid = false;
        float   depth = Math::Infinity;
        Vector3 axis  = Vector3::Zero; // 正規化済み
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
    // OBB 接触 / Ray
    //-------------------------------------------------------------------------
    bool IntersectRayOBB(const Ray& ray,
                         const struct OBB* obb,
                         float& outT) const;

    struct Contact
    {
        bool hit = false;

        float   depth  = 0.0f;
        Vector3 normal = Vector3::UnitY; // A を押し戻す向き
        Vector3 mtv    = Vector3::Zero;  // normal * (depth + eps)
    };

    bool IntersectOBBContact(const OBB* a, const OBB* b, Contact& out) const;

    bool IntersectRayOBB_WithNormal(const Ray& ray,
                                    const struct OBB* obb,
                                    float& outT,
                                    Vector3& outNormal) const;

private:
    std::vector<Polygon> mTerrainPolygons;
    TerrainGrid          mTerrainGrid;

    // Collider は Actor 側が所有。PhysWorld は参照の一覧だけ保持する。
    std::vector<ColliderComponent*> mColliders;
};

} // namespace toy
