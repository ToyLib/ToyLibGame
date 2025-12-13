#include "Asset/Geometry/Polygon.h"
#include <cmath>

namespace toy {

//-----------------------------------------
// Polygon：ワールド座標へ変換
//-----------------------------------------
void Polygon::ComputeWorldTransform(const Matrix4& transform)
{
    offsetA = Vector3::Transform(a, transform);
    offsetB = Vector3::Transform(b, transform);
    offsetC = Vector3::Transform(c, transform);
}

//-----------------------------------------
// レイ vs 三角形（Möller–Trumbore 法）
//-----------------------------------------
bool IntersectRayTriangle(
    const Ray& ray,
    const Vector3& v0,
    const Vector3& v1,
    const Vector3& v2,
    float& outT)
{
    const float epsilon = 1e-5f;

    Vector3 edge1 = v1 - v0;
    Vector3 edge2 = v2 - v0;

    Vector3 h = Vector3::Cross(ray.dir, edge2);
    float a = Vector3::Dot(edge1, h);

    // 平行（背面含む）はヒットしない
    if (fabs(a) < epsilon)
    {
        return false;
    }

    float f = 1.0f / a;
    Vector3 s = ray.start - v0;
    float u = f * Vector3::Dot(s, h);

    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }
    
    Vector3 q = Vector3::Cross(s, edge1);
    float v = f * Vector3::Dot(ray.dir, q);

    if (v < 0.0f || u + v > 1.0f)
    {
        return false;
    }
    
    float t = f * Vector3::Dot(edge2, q);

    if (t > epsilon)
    {
        outT = t;
        return true;
    }

    return false;
}

} // namespace toy
