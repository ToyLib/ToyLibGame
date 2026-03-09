#include "Engine/Debug/DebugDrawSystem.h"

namespace toy
{

void DebugDrawSystem::Clear()
{
    mLines.clear();
}

void DebugDrawSystem::AddLine(const Vector3& a,
                              const Vector3& b)
{
    DebugLine line {};
    line.a = a;
    line.b = b;
    mLines.emplace_back(line);
}

void DebugDrawSystem::AddRay(const Vector3& origin,
                             const Vector3& dir,
                             float length)
{
    Vector3 d = dir;
    if (d.LengthSq() <= Math::NearZeroEpsilon || length <= 0.0f)
    {
        return;
    }

    d.Normalize();

    AddLine(origin, origin + d * length);
}

void DebugDrawSystem::AddBox(const Vector3& min,
                             const Vector3& max)
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
    AddLine(v0, v1);
    AddLine(v1, v2);
    AddLine(v2, v3);
    AddLine(v3, v0);

    // top
    AddLine(v4, v5);
    AddLine(v5, v6);
    AddLine(v6, v7);
    AddLine(v7, v4);

    // vertical
    AddLine(v0, v4);
    AddLine(v1, v5);
    AddLine(v2, v6);
    AddLine(v3, v7);
}

void DebugDrawSystem::AddSphere(const Vector3& center,
                                float radius,
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

        AddLine(p0, p1);
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

        AddLine(p0, p1);
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

        AddLine(p0, p1);
    }
}

void DebugDrawSystem::AddArrow(const Vector3& origin,
                               const Vector3& dir,
                               float length)
{
    Vector3 d = dir;
    if (d.LengthSq() <= Math::NearZeroEpsilon || length <= 0.0f)
    {
        return;
    }

    d.Normalize();

    const Vector3 tip = origin + d * length;

    AddLine(origin, tip);

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

    AddLine(tip, tip + back + wingR);
    AddLine(tip, tip + back - wingR);
    AddLine(tip, tip + back + wingU);
    AddLine(tip, tip + back - wingU);
}

} // namespace toy
