#pragma once

#include "Utils/MathUtil.h"

namespace toy::kit
{

class DebugDrawSystem;

class DebugDraw
{
public:
    static void Initialize();
    static void Shutdown();

    static void Clear();

    static void Line(const Vector3& a,
                     const Vector3& b,
                     const Vector3& color = Vector3(1.0f, 1.0f, 1.0f));

    static void Ray(const Vector3& origin,
                    const Vector3& dir,
                    float length,
                    const Vector3& color = Vector3(1.0f, 1.0f, 1.0f));

    static void Box(const Vector3& min,
                    const Vector3& max,
                    const Vector3& color = Vector3(1.0f, 1.0f, 1.0f));

    static void Sphere(const Vector3& center,
                       float radius,
                       const Vector3& color = Vector3(1.0f, 1.0f, 1.0f),
                       int segments = 16);

    static void Arrow(const Vector3& origin,
                      const Vector3& dir,
                      float length,
                      const Vector3& color = Vector3(1.0f, 1.0f, 1.0f));

    static DebugDrawSystem* GetSystem();

private:
    static DebugDrawSystem* sSystem;
};

} // namespace toy::kit
