#pragma once

#include "Utils/MathUtil.h"

#include <vector>

namespace toy
{

struct DebugLine
{
    Vector3 a { 0.0f, 0.0f, 0.0f };
    Vector3 b { 0.0f, 0.0f, 0.0f };
};

class DebugDrawSystem
{
public:
    void Clear();

    void AddLine(const Vector3& a,
                 const Vector3& b);

    void AddRay(const Vector3& origin,
                const Vector3& dir,
                float length);

    void AddBox(const Vector3& min,
                const Vector3& max);

    void AddSphere(const Vector3& center,
                   float radius,
                   int segments);

    void AddArrow(const Vector3& origin,
                  const Vector3& dir,
                  float length);

    const std::vector<DebugLine>& GetLines() const
    {
        return mLines;
    }

    bool IsEmpty() const
    {
        return mLines.empty();
    }

private:
    std::vector<DebugLine> mLines;
};

} // namespace toy
