#pragma once

#include "Physics/ColliderComponent.h"
#include "Asset/Geometry/Polygon.h"

namespace toy {

//------------------------------------------------------------------------------
// LaserColliderComponent
//------------------------------------------------------------------------------
// ・レーザーやビームのように「線（Ray）」で判定する特殊コライダー。
// ・通常のAABB/OBB ではなく、PhysWorld で Ray vs Polygon の衝突テストを行う前提。
// ・GetRay() をオーバーライドし、射出地点と方向を返す。
// ・mLength は描画時のビーム長としても使用できる（当たり判定自体は Ray ベース）。
//------------------------------------------------------------------------------
class LaserColliderComponent : public ColliderComponent
{
public:
    LaserColliderComponent(class Actor* a);
    
    // レーザー衝突判定用 Ray を返す
    Ray GetRay() const override;
    
    // レーザーの見た目／射程の長さ（実際の当たり判定は無限 Ray でもよい）
    void SetRayLength(float len) { mLength = len; }
    float GetRayLength() const { return mLength; }
    
private:
    // 描画や射程制限で使うレーザーの長さ
    float mLength { 100.0f };
};

} // namespace toy
