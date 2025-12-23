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
    Vector3 a;   // ローカル頂点
    Vector3 b;
    Vector3 c;

    Vector3 offsetA;  // ワールド変換後の頂点（衝突判定用）
    Vector3 offsetB;
    Vector3 offsetC;

    // モデル行列（ワールド行列）を適用 → offset に入れる
    void ComputeWorldTransform(const Matrix4& transform);
};

//==============================
// AABB（Axis-Aligned Bounding Box）
//==============================
struct Cube
{
    Vector3 min;
    Vector3 max;
};
/*
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
 */
/*
//=====================================================
// レイ vs 三角形（Möller–Trumbore 法）
//=====================================================
// ・outT には ray.start からの距離 t が返る
bool IntersectRayTriangle(
    const Ray& ray,
    const Vector3& v0,
    const Vector3& v1,
    const Vector3& v2,
    float& outT);
*/
} // namespace toy
