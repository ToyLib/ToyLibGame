#include "Engine/Debug/DebugDraw.h"
#include "Engine/Debug/DebugDrawSystem.h"

namespace toy
{

DebugDrawSystem* DebugDraw::sSystem = nullptr;

void DebugDraw::Initialize()
{
    if (!sSystem)
    {
        sSystem = new DebugDrawSystem();
    }
}

void DebugDraw::Shutdown()
{
    delete sSystem;
    sSystem = nullptr;
}

void DebugDraw::Clear()
{
    if (sSystem)
    {
        sSystem->Clear();
    }
}

void DebugDraw::Line(const Vector3& a,
                     const Vector3& b,
                     const Vector3& color)
{
    if (sSystem)
    {
        sSystem->AddLine(a, b, color);
    }
}

void DebugDraw::Ray(const Vector3& origin,
                    const Vector3& dir,
                    float length,
                    const Vector3& color)
{
    if (sSystem)
    {
        sSystem->AddRay(origin, dir, length, color);
    }
}

void DebugDraw::Box(const Vector3& min,
                    const Vector3& max,
                    const Vector3& color)
{
    if (sSystem)
    {
        sSystem->AddBox(min, max, color);
    }
}

void DebugDraw::Sphere(const Vector3& center,
                       float radius,
                       const Vector3& color,
                       int segments)
{
    if (sSystem)
    {
        sSystem->AddSphere(center, radius, color, segments);
    }
}

void DebugDraw::Arrow(const Vector3& origin,
                      const Vector3& dir,
                      float length,
                      const Vector3& color)
{
    if (sSystem)
    {
        sSystem->AddArrow(origin, dir, length, color);
    }
}

DebugDrawSystem* DebugDraw::GetSystem()
{
    return sSystem;
}

} // namespace toy
