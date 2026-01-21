#pragma once

#include "Utils/MathUtil.h"

namespace toy {

//==============================
// Polygon（三角形）
//==============================
// ・ローカル座標 a/b/c を保持
// ・ComputeWorldTransform() でワールド座標 offsetA/B/C を計算
struct Polygon
{
    Vector3 a{};   // ローカル頂点
    Vector3 b{};
    Vector3 c{};

    Vector3 offsetA{};  // ワールド変換後の頂点（衝突判定用）
    Vector3 offsetB{};
    Vector3 offsetC{};

    // モデル行列（ワールド行列）を適用 → offset に入れる
    void ComputeWorldTransform(const Matrix4& transform);
};

//==============================
// AABB（Axis-Aligned Bounding Box）
//==============================
struct Cube
{
    Vector3 min{};
    Vector3 max{};
};

} // namespace toy
