#pragma once

#include "Utils/MathUtil.h"

namespace toy
{

class DebugDraw
{
public:
    static void Initialize();
    static void Shutdown();

    static void Clear();

    static void Line(const Vector3& a,
                     const Vector3& b);

    static void Ray(const Vector3& origin,
                    const Vector3& dir,
                    float length);

    static void Box(const Vector3& min,
                    const Vector3& max);

    static void Sphere(const Vector3& center,
                       float radius,
                       int segments = 16);

    static void Arrow(const Vector3& origin,
                      const Vector3& dir,
                      float length);

    static class DebugDrawSystem* GetSystem();

private:
    static class DebugDrawSystem* sSystem;
};

} // namespace toy
