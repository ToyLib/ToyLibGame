// ハイブリッド方式の GetNearestGroundY に対応した PhysWorld
#pragma once

#include "Utils/MathUtil.h"
#include "Physics/ColliderComponent.h"
#include <vector>

namespace toy {

//------------------------------------------------------------------------------
// MTV（最小移動ベクトル）結果
//  - axis : 押し戻し方向（衝突面の法線方向）
//  - depth: めり込み量
//  - valid: true のとき有効
//------------------------------------------------------------------------------
struct MTVResult
{
    Vector3 axis  = Vector3::Zero;
    float   depth = Math::Infinity;
    bool    valid = false;
};


//------------------------------------------------------------------------------
// PhysWorld
//------------------------------------------------------------------------------
// ・ColliderComponent を集約し、衝突判定／押し戻し／地面判定を行う。
// ・AABB / OBB / BoundingSphere、Polygon（地形メッシュ）を扱う。
// ・Ray vs OBB / Ray vs Polygon もサポート。
// ・GetNearestGroundY() は Collider（C_GROUND）と TerrainPolygon の両方を使う
//   “ハイブリッド地面判定”。
//------------------------------------------------------------------------------
class PhysWorld
{
public:
    PhysWorld();
    ~PhysWorld();
    
    //--------------------------------------------------------------------------
    // デバッグ / 全体テスト
    //--------------------------------------------------------------------------
    void ComputeGroundHeight();   // 未使用だが、地形高さのプリ計算などに利用予定
    void Test();                  // 毎フレームの衝突ペアチェック（メイン処理）
    
    //--------------------------------------------------------------------------
    // コライダー管理
    //--------------------------------------------------------------------------
    void AddCollider(class ColliderComponent* c);
    void RemoveCollider(class ColliderComponent* c);
    
    //--------------------------------------------------------------------------
    // 地面情報インターフェイス
    //  - GetGroundHeightAt : TerrainPolygon のみを対象に高さを返す
    //  - GetNearestGroundY : Terrain と C_GROUND の両方から最も近い地面を探索
    //--------------------------------------------------------------------------
    float GetGroundHeightAt(const Vector3& pos) const;
    
    // Actor の足元から見て最も近い地面の Y を取得する（C_GROUND & Terrain 両対応）
    bool GetNearestGroundY(const class Actor* a, float& outY) const;
    
    // 地形ポリゴンをセット（外部メッシュから読み込む三角形配列）
    void SetGroundPolygons(const std::vector<struct Polygon>& polys);
    
    //--------------------------------------------------------------------------
    // RayCCD / RayCast 系
    //--------------------------------------------------------------------------
    // 移動ライン（start→end）が壁に当たる場合、そこで止まる位置を計算
    bool RayHitWall(const Vector3& start,
                    const Vector3& end,
                    Vector3& hitPos) const;
    
    // Ray と OBB の交差（t 値を返す）
    bool IntersectRayOBB(const Ray& ray,
                         const struct OBB* obb,
                         float& outT) const;
    
    //--------------------------------------------------------------------------
    // 衝突判定コールバック
    //  - flagA & flagB の組み合わせで衝突ペアを探索
    //  - doPushBack        : 押し戻し（MTV）による位置補正を行うか
    //  - allowY            : Y 方向にも押し戻す（通常は false で壁ずりしやすく）
    //  - stopVerticalSpeed : 垂直速度を止める（床に着地した瞬間など）
    //--------------------------------------------------------------------------
    void CollideAndCallback(uint32_t flagA,
                            uint32_t flagB,
                            bool doPushBack = false,
                            bool allowY = false,
                            bool stopVerticalSpeed = false);
    
    //--------------------------------------------------------------------------
    // 汎用レイキャスト
    //  - origin + dir から maxDist までレイを飛ばす（dir は正規化前でも可）
    //  - flagMask と & が立つ Collider の OBB を対象に、最も近いヒットを outHit に返す。
    //--------------------------------------------------------------------------
    bool Raycast(const Vector3& origin,
                 const Vector3& dir,
                 float maxDist,
                 uint32_t flagMask,
                 RaycastHit& outHit) const;
    
    //--------------------------------------------------------------------------
    // コライダー数のGetter
    //  - デバッグ等で使用
    //--------------------------------------------------------------------------
    int GetColliderCount() const { return static_cast<int>(mColliders.size()); }

private:
    //--------------------------------------------------------------------------
    // 基本衝突判定（OBB / Sphere）
    //--------------------------------------------------------------------------
    bool CompareLengthOBB(const struct OBB* cA,
                          const struct OBB* cB,
                          const Vector3& vSep,
                          const Vector3& vDistance);
    
    bool JudgeWithOBB(class ColliderComponent* col1,
                      class ColliderComponent* col2);
    
    bool IsCollideBoxOBB(const OBB* cA,
                         const OBB* cB);
    
    bool JudgeWithRadius(class ColliderComponent* col1,
                         class ColliderComponent* col2);
    
    //--------------------------------------------------------------------------
    // MTV（押し戻し）関連
    //--------------------------------------------------------------------------
    Vector3 ComputePushBackDirection(class ColliderComponent* a,
                                     class ColliderComponent* b,
                                     bool allowY);
    
    bool CompareLengthOBB_MTV(const OBB* cA,
                              const OBB* cB,
                              const Vector3& vSep,
                              const Vector3& vDistance,
                              MTVResult& mtv);
    
    bool IsCollideBoxOBB_MTV(const OBB* cA,
                             const OBB* cB,
                             MTVResult& mtv);
    
    //--------------------------------------------------------------------------
    // 地形ポリゴン 判定
    //--------------------------------------------------------------------------
    // ポリゴン内か判定（XZ 平面）
    bool IsInPolygon(const struct Polygon* pl,
                     const struct Vector3& p) const;
    
    // ポリゴン上の高さ（Y）を返す
    float PolygonHeight(const struct Polygon* pl,
                        const struct Vector3& p) const;
    
    // Actor に紐づく C_FOOT コライダーを取得
    class ColliderComponent* FindFootCollider(const Actor* a) const;
    
    //--------------------------------------------------------------------------
    // 保持データ
    //--------------------------------------------------------------------------
    std::vector<class ColliderComponent*> mColliders; // すべてのコライダー
    std::vector<struct Polygon> mTerrainPolygons;     // 静的地形メッシュ（三角形配列）
};

} // namespace toy


///// memo
// toy::physmathなどのユーティリティ化候補
//    IntersectRayOBB
//    IntersectRayTriangle
//    IsInPolygon
//    PolygonHeight
//    など
