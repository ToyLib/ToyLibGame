#include "Physics/PhysWorld.h"
#include "Engine/Core/Application.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Core/Actor.h"
#include "Camera/CameraComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Asset/Geometry/Polygon.h"
#include "Physics/ColliderComponent.h"
#include "Movement/MoveComponent.h"
#include "Utils/MathUtil.h"

#include <algorithm>

namespace toy {

PhysWorld::PhysWorld()
{
}

PhysWorld::~PhysWorld()
{
}

//------------------------------------------------------------------------------
// Test
//------------------------------------------------------------------------------
// ・毎フレーム呼び出される想定の「メイン衝突処理」。
// ・Collider のバッファをクリア → 各種ペアの衝突判定を行う。
// ・C_PLAYER vs C_ENEMY / C_BULLET は「ヒットのみ」。
// ・C_ENEMY vs C_WALL は押し戻しあり。
// ・C_LASER vs C_ENEMY は Ray vs Mesh（Polygon 配列）で判定。
//------------------------------------------------------------------------------
void PhysWorld::Test()
{
    // まず全コライダーのヒットバッファをクリア
    for (auto& c : mColliders)
    {
        c->ClearCollidBuffer();
    }
    
    // 通常のコリジョン（OBB & 半径判定）
    CollideAndCallback(C_PLAYER, C_ENEMY);                      // プレイヤー vs 敵
    CollideAndCallback(C_PLAYER, C_BULLET);                     // プレイヤー vs 弾
    CollideAndCallback(C_ENEMY, C_WALL, true, false);           // 敵 vs 壁（押し戻し）
    
    //--------------------------------------------------------------------------
    // Laser vs Enemy（Ray vs Mesh）
    //  - C_LASER フラグを持つコライダーが発射する Ray と
    //    C_ENEMY フラグのメッシュポリゴンとの衝突判定。
    //--------------------------------------------------------------------------
    for (auto& c1 : mColliders)
    {
        if (!c1->HasFlag(C_LASER)) continue;
        if (!c1->GetDisp())        continue;
        
        // LaserColliderComponent が返す Ray
        Ray ray = c1->GetRay();
        
        for (auto& c2 : mColliders)
        {
            if (c1 == c2)              continue;
            if (!c2->HasFlag(C_ENEMY)) continue;
            if (!c2->GetDisp())        continue;
            
            const auto& polygons = c2->GetBoundingVolume()->GetPolygons(); // Polygon 配列
            bool   hit      = false;
            float  closestT = Math::Infinity;
            Vector3 hitPoint;
            
            // NUM_VERTEX は BoundingVolumeComponent 側で生成した三角形数
            for (int i = 0; i < NUM_VERTEX; ++i)
            {
                const auto& poly = polygons[i];
                float t;
                
                // レイと三角形の交差（t が距離）
                if (IntersectRayTriangle(ray, poly.a, poly.b, poly.c, t))
                {
                    if (t < closestT)
                    {
                        closestT = t;
                        hit      = true;
                        hitPoint = ray.start + ray.dir * t;
                    }
                }
            }
            
            if (hit)
            {
                // ヒットしたら相互に衝突登録
                c1->Collided(c2);
                c2->Collided(c1);
                // hitPoint はダメージエフェクト等の座標に利用可能
            }
        }
    }
}

//------------------------------------------------------------------------------
// OBB の投影比較（分離軸定理 SAT）
//------------------------------------------------------------------------------
// ・vSep      : 分離軸
// ・vDistance : 中心同士の距離ベクトル
// ・A/B の OBB を vSep 上に投影し、投影長の合計より距離が大きければ「分離」。
//------------------------------------------------------------------------------
bool PhysWorld::CompareLengthOBB(const OBB* cA, const OBB* cB,
                                 const Vector3& vSep, const Vector3& vDistance)
{
    // 分離軸上の A と B の中心距離
    float length = fabsf(Vector3::Dot(vSep, vDistance));
    
    // A の半径を vSep 上に投影
    float lenA =
        fabs(Vector3::Dot(cA->axisX, vSep) * cA->radius.x)
      + fabs(Vector3::Dot(cA->axisY, vSep) * cA->radius.y)
      + fabs(Vector3::Dot(cA->axisZ, vSep) * cA->radius.z);
    
    // B の半径を vSep 上に投影
    float lenB =
        fabs(Vector3::Dot(cB->axisX, vSep) * cB->radius.x)
      + fabs(Vector3::Dot(cB->axisY, vSep) * cB->radius.y)
      + fabs(Vector3::Dot(cB->axisZ, vSep) * cB->radius.z);
    
    // 距離 > (A+B の投影長) なら分離している
    if (length > lenA + lenB)
    {
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
// JudgeWithOBB
//------------------------------------------------------------------------------
// ・Collider から OBB を取り出し、IsCollideBoxOBB に委譲。
//------------------------------------------------------------------------------
bool PhysWorld::JudgeWithOBB(ColliderComponent* col1, ColliderComponent* col2)
{
    auto obb1 = col1->GetBoundingVolume()->GetOBB();
    auto obb2 = col2->GetBoundingVolume()->GetOBB();
    
    // OBB 同士の衝突判定
    return IsCollideBoxOBB(obb1.get(), obb2.get());
}

//------------------------------------------------------------------------------
// IsCollideBoxOBB
//------------------------------------------------------------------------------
// ・SAT による OBB vs OBB 判定。
// ・A/B の各軸 + それぞれの外積 で 15 本の分離軸をチェック。
// ・どれか 1 本でも「分離」していれば非衝突。
//------------------------------------------------------------------------------
bool PhysWorld::IsCollideBoxOBB(const OBB* cA, const OBB* cB)
{
    // 中心間の距離ベクトル
    Vector3 vDistance = cB->pos - cA->pos;
    
    // 各ローカル軸を分離軸として比較
    if (!CompareLengthOBB(cA, cB, cA->axisX, vDistance)) return false;
    if (!CompareLengthOBB(cA, cB, cA->axisY, vDistance)) return false;
    if (!CompareLengthOBB(cA, cB, cA->axisZ, vDistance)) return false;
    if (!CompareLengthOBB(cA, cB, cB->axisX, vDistance)) return false;
    if (!CompareLengthOBB(cA, cB, cB->axisY, vDistance)) return false;
    if (!CompareLengthOBB(cA, cB, cB->axisZ, vDistance)) return false;
    
    // 各軸同士の外積も分離軸としてチェック
    Vector3 vSep = Vector3::Cross(cA->axisX, cB->axisX);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    vSep = Vector3::Cross(cA->axisX, cB->axisY);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    vSep = Vector3::Cross(cA->axisX, cB->axisZ);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    vSep = Vector3::Cross(cA->axisY, cB->axisX);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    vSep = Vector3::Cross(cA->axisY, cB->axisY);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    vSep = Vector3::Cross(cA->axisY, cB->axisZ);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    vSep = Vector3::Cross(cA->axisZ, cB->axisX);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    vSep = Vector3::Cross(cA->axisZ, cB->axisY);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    vSep = Vector3::Cross(cA->axisZ, cB->axisZ);
    if (!CompareLengthOBB(cA, cB, vSep, vDistance)) return false;
    
    // すべての軸で分離していなければ衝突
    return true;
}

//------------------------------------------------------------------------------
// JudgeWithRadius
//------------------------------------------------------------------------------
// ・まずはスフィア同士で早期判定するための軽量チェック。
// ・中心距離 < 半径の和 なら「衝突の可能性あり」。
//------------------------------------------------------------------------------
bool PhysWorld::JudgeWithRadius(class ColliderComponent* col1,
                                class ColliderComponent* col2)
{
    auto distance = col1->GetPosition() - col2->GetPosition();
    float len = distance.Length();
    float threshold =
        col1->GetBoundingVolume()->GetRadius() +
        col2->GetBoundingVolume()->GetRadius();
    
    return (threshold > len);
}

//------------------------------------------------------------------------------
// コライダー管理
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// IsInPolygon
//------------------------------------------------------------------------------
// ・XZ平面に投影した三角形に点 p が含まれるかどうかを判定。
// ・三辺すべてについて、外積の向きが同じなら「内側」とみなす。
//------------------------------------------------------------------------------
bool PhysWorld::IsInPolygon(const Polygon* pl, const Vector3& p) const
{
    if (((pl->b.x - pl->a.x) * (p.z - pl->a.z) -
         (pl->b.z - pl->a.z) * (p.x - pl->a.x)) < 0)
    {
        return false;
    }
    if (((pl->c.x - pl->b.x) * (p.z - pl->b.z) -
         (pl->c.z - pl->b.z) * (p.x - pl->b.x)) < 0)
    {
        return false;
    }
    if (((pl->a.x - pl->c.x) * (p.z - pl->c.z) -
         (pl->a.z - pl->c.z) * (p.x - pl->c.x)) < 0)
    {
        return false;
    }
    
    return true;
}

//------------------------------------------------------------------------------
// PolygonHeight
//------------------------------------------------------------------------------
// ・XZ平面上の点 p に対して、その三角形ポリゴン上の Y 高さを返す。
// ・三点 a,b,c から平面方程式を求めて高さを算出。
//------------------------------------------------------------------------------
float PhysWorld::PolygonHeight(const Polygon* pl, const Vector3& p) const
{
    float wa, wb, wc;    // 平面方程式の係数
    
    wa = (pl->c.z - pl->a.z) * (pl->b.y - pl->a.y) -
         (pl->c.y - pl->a.y) * (pl->b.z - pl->a.z);
    wb = (pl->c.y - pl->a.y) * (pl->b.x - pl->a.x) -
         (pl->c.x - pl->a.x) * (pl->b.y - pl->a.y);
    wc = (pl->c.x - pl->a.x) * (pl->b.z - pl->a.z) -
         (pl->c.z - pl->a.z) * (pl->b.x - pl->a.x);
    
    // 平面方程式から y を解く
    return -(wa * (p.x - pl->a.x) + wb * (p.z - pl->a.z)) / wc + pl->a.y;
}

//------------------------------------------------------------------------------
// CompareLengthOBB_MTV
//------------------------------------------------------------------------------
// ・CompareLengthOBB の MTV 版。
// ・overlap（重なり量）が負なら非衝突。
// ・最小の overlap を持つ軸を MTVResult に記録する。
//------------------------------------------------------------------------------
bool PhysWorld::CompareLengthOBB_MTV(const OBB* cA, const OBB* cB,
                                     const Vector3& vSep,
                                     const Vector3& vDistance,
                                     MTVResult& mtv)
{
    // ほぼゼロ長の軸は無視
    if (vSep.LengthSq() < 1e-6f) return true;
    
    float length = fabsf(Vector3::Dot(vSep, vDistance));
    
    float lenA =
        fabs(Vector3::Dot(cA->axisX, vSep) * cA->radius.x) +
        fabs(Vector3::Dot(cA->axisY, vSep) * cA->radius.y) +
        fabs(Vector3::Dot(cA->axisZ, vSep) * cA->radius.z);
    
    float lenB =
        fabs(Vector3::Dot(cB->axisX, vSep) * cB->radius.x) +
        fabs(Vector3::Dot(cB->axisY, vSep) * cB->radius.y) +
        fabs(Vector3::Dot(cB->axisZ, vSep) * cB->radius.z);
    
    float overlap = lenA + lenB - length;
    
    // overlap < 0 ならこの軸では分離 → 非衝突
    if (overlap < 0.0f)
    {
        return false;
    }
    
    // MTV（最小のめり込み量）を更新
    if (overlap < mtv.depth)
    {
        mtv.depth = overlap;
        mtv.axis  = vSep;
        mtv.valid = true;
    }
    
    return true;
}

//------------------------------------------------------------------------------
// IsCollideBoxOBB_MTV
//------------------------------------------------------------------------------
// ・SAT 判定しつつ、最も浅いめり込み軸を MTVResult に格納。
// ・全軸で分離していなければ true を返す。
//------------------------------------------------------------------------------
bool PhysWorld::IsCollideBoxOBB_MTV(const OBB* cA, const OBB* cB, MTVResult& mtv)
{
    Vector3 vDistance = cB->pos - cA->pos;
    
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
// ・2 つの Collider の OBB から MTV を求め、押し戻しベクトルを返す。
// ・allowY = false のとき Y 成分を 0 にして「壁ずり」しやすくする。
// ・MTV が取れなかった場合は距離ベクトルベースのフォールバック。
//------------------------------------------------------------------------------
Vector3 PhysWorld::ComputePushBackDirection(ColliderComponent* a,
                                            ColliderComponent* b,
                                            bool allowY)
{
    MTVResult mtv;
    
    auto obb1 = a->GetBoundingVolume()->GetOBB();
    auto obb2 = b->GetBoundingVolume()->GetOBB();
    
    // SAT + MTV 計算
    if (!IsCollideBoxOBB_MTV(obb1.get(), obb2.get(), mtv) || !mtv.valid)
    {
        // フォールバック：単純に a→b 方向に少し押し戻す
        Vector3 delta = a->GetPosition() - b->GetPosition();
        if (!allowY) delta.y = 0.0f;
        
        if (delta.LengthSq() > Math::NearZeroEpsilon)
        {
            delta.Normalize();
        }
        else
        {
            // 完全に同位置などで方向が出ないときの適当な軸
            delta = Vector3::UnitZ;
        }
        return delta * 0.1f;
    }
    
    // MTV に基づく押し戻し
    Vector3 pushAxis = mtv.axis;
    if (!allowY) pushAxis.y = 0.0f;
    
    // a の視点から見て「外側を向く」ように向きを補正
    Vector3 dirAB = a->GetPosition() - b->GetPosition();
    if (Vector3::Dot(pushAxis, dirAB) < 0.0f)
    {
        pushAxis *= -1.0f;
    }
    
    if (pushAxis.LengthSq() > Math::NearZeroEpsilon)
    {
        pushAxis.Normalize();
        // 少しだけ余裕をもって押し出す
        return pushAxis * (mtv.depth + 0.05f);
    }
    
    return Vector3::Zero;
}

//------------------------------------------------------------------------------
// GetNearestGroundY
//------------------------------------------------------------------------------
// ・Actor の「足」コライダー（C_FOOT）を基準に、
//   - C_GROUND コライダー
//   - TerrainPolygon（メッシュ）
//   の両方から「一番近い地面の高さ」を探すハイブリッド方式。
//------------------------------------------------------------------------------
bool PhysWorld::GetNearestGroundY(const Actor* a, float& outY) const
{    GroundHit hit;
    if (!GetNearestGroundHit(a, hit)) return false;
    outY = hit.y;
    return true;
}

bool PhysWorld::GetNearestGroundHit(const Actor* a, GroundHit& outHit) const
{
    outHit = GroundHit{};
    if (!a) return false;

    // 足元の Collider（C_FOOT）
    const auto* foot = FindFootCollider(a);
    if (!foot) return false;

    const Cube box = foot->GetBoundingVolume()->GetWorldAABB();
    const float footY = box.min.y;

    float highest = -std::numeric_limits<float>::max();
    bool  found   = false;

    // 候補の情報を保持（highestに対応するもの）
    GroundSource bestSource = GroundSource::None;
    const ColliderComponent* bestCol = nullptr;
    Vector3 bestPos = Vector3::Zero;
    Vector3 bestNormal = Vector3::UnitY;

    //--------------------------------------------------------------------------
    // 1) C_GROUND コライダー（既存と同じ：最も高い地面）
    //--------------------------------------------------------------------------
    for (auto& c : mColliders)
    {
        if (!c->HasFlag(C_GROUND)) continue;
        if (c->GetOwner() == a)    continue;

        const Cube other = c->GetBoundingVolume()->GetWorldAABB();

        const bool xzOverlap =
            box.max.x > other.min.x && box.min.x < other.max.x &&
            box.max.z > other.min.z && box.min.z < other.max.z;

        const float yGap = footY - other.max.y;

        if (xzOverlap && yGap > 0.0f && highest < other.max.y)
        {
            highest = other.max.y;
            found   = true;

            bestSource = GroundSource::Collider;
            bestCol    = c;
            bestPos    = Vector3((box.min.x + box.max.x) * 0.5f, highest,
                                 (box.min.z + box.max.z) * 0.5f);
            bestNormal = Vector3::UnitY; // コライダー床はとりあえず上向き
        }
    }

    //--------------------------------------------------------------------------
    // 2) TerrainPolygon（既存と同じ：Actor中心で高さ取得）
    //--------------------------------------------------------------------------
    const Vector3 center = a->GetPosition();
    const float terrainY = GetGroundHeightAt(center);

    if (terrainY > highest)
    {
        highest = terrainY;
        found   = true;

        bestSource = GroundSource::Terrain;
        bestCol    = nullptr;
        bestPos    = Vector3(center.x, terrainY, center.z);

        // normalは最小実装として UnitY でもOKだが、
        // “GroundHitを返す意味”が出るので、可能なら後で埋める前提でいったんUnitY
        bestNormal = Vector3::UnitY;
    }

    if (!found) return false;

    outHit.hit     = true;
    outHit.y       = highest;
    outHit.pos     = bestPos;
    outHit.normal  = bestNormal;
    outHit.source  = bestSource;
    outHit.collider = bestCol;
    outHit.yGap    = footY - highest;

    return true;
}

//------------------------------------------------------------------------------
// GetGroundHeightAt
//------------------------------------------------------------------------------
// ・XZ 座標 pos を与えて、TerrainPolygon ベースの地表高さを返す。
// ・該当ポリゴンがない場合は -∞ に近い値を返す（呼び出し側で扱う）。
//------------------------------------------------------------------------------
float PhysWorld::GetGroundHeightAt(const Vector3& pos) const
{
    float highestY = -std::numeric_limits<float>::max();
    for (const auto& poly : mTerrainPolygons)
    {
        if (IsInPolygon(&poly, pos))
        {
            float y = PolygonHeight(&poly, pos);
            if (y > highestY)
            {
                highestY = y;
            }
        }
    }
    return highestY;
}

//------------------------------------------------------------------------------
// SetGroundPolygons
//------------------------------------------------------------------------------
// ・外部の地形メッシュ（頂点を三角形分割済み）を登録。
//------------------------------------------------------------------------------
void PhysWorld::SetGroundPolygons(const std::vector<Polygon>& polys)
{
    mTerrainPolygons = polys;
}

//------------------------------------------------------------------------------
// CollideAndCallback
//------------------------------------------------------------------------------
// ・指定フラグの組み合わせで、全コライダーのペアを走査。
// ・JudgeWithRadius → JudgeWithOBB で衝突判定。
// ・doPushBack = true の場合、MTV による押し戻しを行う。
// ・stopVerticalSpeed = true の場合、MoveComponent の垂直速度を 0 にする。
//------------------------------------------------------------------------------
void PhysWorld::CollideAndCallback(uint32_t flagA,
                                   uint32_t flagB,
                                   bool doPushBack,
                                   bool allowY,
                                   bool stopVerticalSpeed)
{
    for (auto& c1 : mColliders)
    {
        if (!c1->GetDisp() || !c1->HasFlag(flagA)) continue;
        
        Vector3 totalPush = Vector3::Zero;
        bool    collided  = false;
        
        for (auto& c2 : mColliders)
        {
            if (!c2->GetDisp() || !c2->HasFlag(flagB)) continue;
            if (c1->GetOwner() == c2->GetOwner())      continue;
            
            // スフィアで早期判定 → OBB で精密判定
            if (JudgeWithRadius(c1, c2) && JudgeWithOBB(c1, c2))
            {
                c1->Collided(c2);
                c2->Collided(c1);
                collided = true;
                
                // 押し戻しが必要なら MTV を累積
                if (doPushBack)
                {
                    Vector3 push = ComputePushBackDirection(c1, c2, allowY);
                    totalPush += push;
                }
            }
        }
        
        // 衝突していて押し戻し指定がある場合、位置をまとめて補正
        if (collided && doPushBack)
        {
            Vector3 newPos = c1->GetOwner()->GetPosition() + totalPush;
            c1->GetOwner()->SetPosition(newPos);
            
            // 垂直速度を止める（床に着地したケースなど）
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
// FindFootCollider
//------------------------------------------------------------------------------
// ・Actor が持つ ColliderComponent の中から、C_FOOT の付いたものを探す。
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
// IntersectRayOBB
//------------------------------------------------------------------------------
// ・Ray vs OBB の交差判定。
// ・Ray を OBB のローカル軸上に射影し、各スラブとの交差区間 [tMin,tMax] を求める。
// ・tMin > tMax になれば非交差。
//------------------------------------------------------------------------------
bool PhysWorld::IntersectRayOBB(const Ray& ray, const OBB* obb, float& outT) const
{
    const float epsilon = 1e-6f;
    Vector3 p = obb->pos - ray.start;
    float tMin = 0.0f;
    float tMax = Math::Infinity;
    
    for (int i = 0; i < 3; ++i)
    {
        Vector3 axis;
        float   r = 0.0f;
        
        if (i == 0) { axis = obb->axisX; r = obb->radius.x; }
        if (i == 1) { axis = obb->axisY; r = obb->radius.y; }
        if (i == 2) { axis = obb->axisZ; r = obb->radius.z; }
        
        float e = Vector3::Dot(axis, p);
        float f = Vector3::Dot(ray.dir, axis);
        
        if (fabsf(f) > epsilon)
        {
            float t1 = (e + r) / f;
            float t2 = (e - r) / f;
            if (t1 > t2) std::swap(t1, t2);
            
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            
            if (tMin > tMax)
            {
                return false;
            }
        }
        else
        {
            // レイが軸に平行な場合、中心投影が [-r, +r] の範囲外なら非交差
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
// ・start→end の線分を Ray にし、C_WALL コライダーの OBB と交差するか判定。
// ・最も近いヒット位置（少し手前）を hitPos に返す。
// ・MoveComponent::TryMoveWithRayCheck で CCD 用に使用。
//------------------------------------------------------------------------------
bool PhysWorld::RayHitWall(const Vector3& start,
                           const Vector3& end,
                           Vector3& hitPos) const
{
    Ray ray;
    ray.start = start;
    ray.dir   = end - start;
    
    float rayLen = ray.dir.Length();
    if (rayLen < Math::NearZeroEpsilon) return false;
    ray.dir.Normalize();
    
    float closestT = rayLen;
    bool  hit      = false;
    
    for (auto& col : mColliders)
    {
        if (!col->HasFlag(C_WALL)) continue;
        auto obb = col->GetBoundingVolume()->GetOBB();
        
        float t;
        if (IntersectRayOBB(ray, obb.get(), t))
        {
            if (t < closestT)
            {
                closestT = t;
                hit      = true;
            }
        }
    }
    
    if (hit)
    {
        // ほんの少し手前で止めて、めり込みを防止
        hitPos = ray.start + ray.dir * (closestT - 0.01f);
        return true;
    }
    
    return false;
}

//------------------------------------------------------------------------------
// Raycast
//------------------------------------------------------------------------------
// ・汎用レイキャスト：origin + dir（正規化前で OK）から maxDist まで飛ばす。
// ・flagMask と & が立つ Collider の OBB を対象に、最も近いヒットを outHit に返す。
// ・outHit.actor にはヒットした Collider の Owner（Actor）が入る。
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
    bool  hit      = false;

    for (auto* col : mColliders)
    {
        if (!col->GetDisp())
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
                // normal が必要なら、ここで OBB 面から算出して設定する
            }
        }
    }

    return hit;
}

} // namespace toy
