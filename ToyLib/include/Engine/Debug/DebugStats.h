#pragma once

namespace toy {

struct DebugStats
{
    float FrameTimeMs     = 0.0f;
    float FPS             = 0.0f;

    int   ActorCount      = 0;
    int   ColliderCount   = 0;
    int   DrawCallCount   = 0;
    int   DrawObjectCount = 0;

    float PhysicsTimeMs   = 0.0f;
    float RenderTimeMs    = 0.0f;
};

} // namespace toy
