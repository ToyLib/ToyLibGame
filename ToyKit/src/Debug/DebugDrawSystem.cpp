#include "Debug/DebugDrawSystem.h"

namespace toy::kit
{

void DebugDrawSystem::Clear()
{
    mLines.clear();
}

void DebugDrawSystem::AddLine(const Vector3& a,
                              const Vector3& b,
                              const Vector3& color)
{
    DebugLine line {};
    line.a = a;
    line.b = b;
    line.color = color;
    mLines.emplace_back(line);
}

void DebugDrawSystem::AddRay(const Vector3& origin,
                             const Vector3& dir,
                             float length,
                             const Vector3& color)
{
    Vector3 d = dir;
    if (d.LengthSq() <= Math::NearZeroEpsilon || length <= 0.0f)
    {
        return;
    }

    d.Normalize();

    AddLine(origin, origin + d * length, color);
}

void DebugDrawSystem::AddBox(const Vector3& min,
                             const Vector3& max,
                             const Vector3& color)
{
    const Vector3 v0(min.x, min.y, min.z);
    const Vector3 v1(max.x, min.y, min.z);
    const Vector3 v2(max.x, min.y, max.z);
    const Vector3 v3(min.x, min.y, max.z);

    const Vector3 v4(min.x, max.y, min.z);
    const Vector3 v5(max.x, max.y, min.z);
    const Vector3 v6(max.x, max.y, max.z);
    const Vector3 v7(min.x, max.y, max.z);

    // bottom
    AddLine(v0, v1, color);
    AddLine(v1, v2, color);
    AddLine(v2, v3, color);
    AddLine(v3, v0, color);

    // top
    AddLine(v4, v5, color);
    AddLine(v5, v6, color);
    AddLine(v6, v7, color);
    AddLine(v7, v4, color);

    // vertical
    AddLine(v0, v4, color);
    AddLine(v1, v5, color);
    AddLine(v2, v6, color);
    AddLine(v3, v7, color);
}

void DebugDrawSystem::AddSphere(const Vector3& center,
                                float radius,
                                const Vector3& color,
                                int segments)
{
    if (radius <= 0.0f)
    {
        return;
    }

    if (segments < 4)
    {
        segments = 4;
    }

    // XY
    for (int i = 0; i < segments; ++i)
    {
        const float t0 = Math::TwoPi * (float)i / (float)segments;
        const float t1 = Math::TwoPi * (float)(i + 1) / (float)segments;

        const Vector3 p0(
            center.x + Math::Cos(t0) * radius,
            center.y + Math::Sin(t0) * radius,
            center.z);

        const Vector3 p1(
            center.x + Math::Cos(t1) * radius,
            center.y + Math::Sin(t1) * radius,
            center.z);

        AddLine(p0, p1, color);
    }

    // XZ
    for (int i = 0; i < segments; ++i)
    {
        const float t0 = Math::TwoPi * (float)i / (float)segments;
        const float t1 = Math::TwoPi * (float)(i + 1) / (float)segments;

        const Vector3 p0(
            center.x + Math::Cos(t0) * radius,
            center.y,
            center.z + Math::Sin(t0) * radius);

        const Vector3 p1(
            center.x + Math::Cos(t1) * radius,
            center.y,
            center.z + Math::Sin(t1) * radius);

        AddLine(p0, p1, color);
    }

    // YZ
    for (int i = 0; i < segments; ++i)
    {
        const float t0 = Math::TwoPi * (float)i / (float)segments;
        const float t1 = Math::TwoPi * (float)(i + 1) / (float)segments;

        const Vector3 p0(
            center.x,
            center.y + Math::Cos(t0) * radius,
            center.z + Math::Sin(t0) * radius);

        const Vector3 p1(
            center.x,
            center.y + Math::Cos(t1) * radius,
            center.z + Math::Sin(t1) * radius);

        AddLine(p0, p1, color);
    }
}

void DebugDrawSystem::AddArrow(const Vector3& origin,
                               const Vector3& dir,
                               float length,
                               const Vector3& color)
{
    Vector3 d = dir;
    if (d.LengthSq() <= Math::NearZeroEpsilon || length <= 0.0f)
    {
        return;
    }

    d.Normalize();

    const Vector3 tip = origin + d * length;

    AddLine(origin, tip, color);

    // 矢印の頭
    const float headLen = length * 0.2f;

    Vector3 refUp = Vector3::UnitY;
    if (Math::Abs(Vector3::Dot(d, refUp)) > 0.98f)
    {
        refUp = Vector3::UnitX;
    }

    Vector3 right = Vector3::Cross(d, refUp);
    if (right.LengthSq() <= Math::NearZeroEpsilon)
    {
        return;
    }
    right.Normalize();

    Vector3 up = Vector3::Cross(right, d);
    if (up.LengthSq() <= Math::NearZeroEpsilon)
    {
        return;
    }
    up.Normalize();

    const Vector3 back = -1.0f * d * headLen;
    const Vector3 wingR = right * (headLen * 0.5f);
    const Vector3 wingU = up    * (headLen * 0.5f);

    AddLine(tip, tip + back + wingR, color);
    AddLine(tip, tip + back - wingR, color);
    AddLine(tip, tip + back + wingU, color);
    AddLine(tip, tip + back - wingU, color);
}

} // namespace toy::kit
