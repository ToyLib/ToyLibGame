#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"
#include "Asset/Geometry/Polygon.h"

#include <array>
#include <vector>
#include <memory>
#include <span>


namespace toy {

//------------------------------------------------------------------------------
// OBB（Oriented Bounding Box / 有向境界ボックス）
//------------------------------------------------------------------------------
// ・pos    : 中心位置（ローカル空間 or ワールド空間）
// ・radius : 各軸方向の「半径」（幅/高さ/奥行きの半分）
// ・rot    : 回転角度っぽい情報（axisX/axisY/axisZ からの簡易パラメータ）
// ・axisX/Y/Z : OBB のローカル軸ベクトル（正規化想定）
// ・min/max : 元となる AABB の最小/最大（ローカル空間）
//------------------------------------------------------------------------------
struct OBB
{
    Vector3 pos;      // 中心座標
    Vector3 radius;   // 半径（各軸方向の半分のサイズ）
    Vector3 rot;      // 回転角度（簡易的な表現）
    Vector3 axisX;    // ローカルX軸
    Vector3 axisY;    // ローカルY軸
    Vector3 axisZ;    // ローカルZ軸
    
    Vector3 min;      // 元 AABB の最小
    Vector3 max;      // 元 AABB の最大
    
    void operator = (const OBB& src)
    {
        OBB::min    = src.min;
        OBB::max    = src.max;
        OBB::pos    = src.pos;
        OBB::radius = src.radius;
        OBB::rot    = src.rot;
        OBB::axisX  = src.axisX;
        OBB::axisY  = src.axisY;
        OBB::axisZ  = src.axisZ;
    }
};


//------------------------------------------------------------------------------
// BoundingVolumeComponent
//------------------------------------------------------------------------------
// ・メッシュから AABB / OBB / ポリゴン情報を生成して保持するコンポーネント。
// ・PhysWorld などの物理判定やデバッグ可視化で利用する。
// ・OBB / AABB / ポリゴンの生成タイミングはゲーム側で明示的に呼び出す想定。
//------------------------------------------------------------------------------
class BoundingVolumeComponent : public Component
{
public:
    static constexpr size_t kNumPolygons = 12;
    // コンストラクタ / デストラクタ
    BoundingVolumeComponent(class Actor* a);
    ~BoundingVolumeComponent();
    
    //--------------------------------------------------------------------------
    // バウンディングボリュームの構築
    //--------------------------------------------------------------------------
    // VertexArray 群から AABB を計算し、OBB/ポリゴンも生成する
    void ComputeBoundingVolume(const std::vector<std::shared_ptr<class VertexArray>>& va);
    
    // Min/Max（ローカル空間）を直接指定して AABB/ポリゴンを生成
    void ComputeBoundingVolume(const Vector3& min, const Vector3& max);
    
    // MeshComponentからAABBポリゴンを生成する
    void ComputeFromMeshComponent(const class MeshComponent* meshComp);
    
    // 手動補正用ヘルパー：
    // ・pos : ボックス全体の平行移動
    // ・sc  : 各軸ごとのスケール倍率
    void AdjustBoundingBox(const Vector3& pos, const Vector3& sc);
    
    // デバッグ表示用ワイヤーフレーム VAO を作成
    void CreateVArray();
    
    // アクターのワールド行列更新時に呼ばれる
    // OBB の中心・軸・半径・バウンディングスフィア半径などを更新
    void OnUpdateWorldTransform() override;
    
    //--------------------------------------------------------------------------
    // ゲッター
    //--------------------------------------------------------------------------
    std::shared_ptr<struct Cube> GetAABB() const { return mBoundingBox; }
    std::shared_ptr<struct OBB>  GetOBB()  const { return mObb; }
    
    // ワールド空間での AABB を取得（位置＋スケール反映）
    struct Cube GetWorldAABB() const;
    
    // AABB から生成した 6面×2tri = 12枚のポリゴン（ローカル空間）
    //（コピー無し・サイズ付き）
    std::span<const struct Polygon> GetPolygons() const
    {
        return { mPolygons.data(), mPolygons.size() };
    }

    // バウンディングスフィア半径（OBB から算出）
    float GetRadius() const { return mRadius; }
    void  SetRadius(float f) { mRadius = f; }
    
private:
    // AABB からポリゴン配列を生成（12三角形）
    void CreatePolygons();
    
    // ローカル空間の AABB
    std::shared_ptr<struct Cube> mBoundingBox;
    
    // OBB（中心・軸・半径など）
    std::shared_ptr<struct OBB>  mObb;
    
    // バウンディングスフィア半径
    float mRadius{};
    
    // AABB から生成した 12枚のポリゴン（ローカル空間）
    std::array<Polygon, kNumPolygons> mPolygons{};
    
    // デバッグ表示用のワイヤーフレーム（AABB 可視化）
    std::unique_ptr<class WireframeComponent> mWireframe;
};



} // namespace toy
