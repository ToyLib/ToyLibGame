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
                     const Vector3& b)
{
    if (sSystem)
    {
        sSystem->AddLine(a, b);
    }
}

void DebugDraw::Ray(const Vector3& origin,
                    const Vector3& dir,
                    float length)
{
    if (sSystem)
    {
        sSystem->AddRay(origin, dir, length);
    }
}

void DebugDraw::Box(const Vector3& min,
                    const Vector3& max)
{
    if (sSystem)
    {
        sSystem->AddBox(min, max);
    }
}

void DebugDraw::Sphere(const Vector3& center,
                       float radius,
                       int segments)
{
    if (sSystem)
    {
        sSystem->AddSphere(center, radius, segments);
    }
}

void DebugDraw::Arrow(const Vector3& origin,
                      const Vector3& dir,
                      float length)
{
    if (sSystem)
    {
        sSystem->AddArrow(origin, dir, length);
    }
}

DebugDrawSystem* DebugDraw::GetSystem()
{
    return sSystem;
}

} // namespace toy
