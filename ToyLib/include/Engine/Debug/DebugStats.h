#pragma once

namespace toy {

struct DebugStats
{
    float DeltaTimeMs        { 0.0f };
    float FPS                { 0.0f };

    int   ActorCount         { 0 };
    int   ColliderCount      { 0 };
    int   DrawCallCount      { 0 };
    int   DrawObjectCount    { 0 };
    int   OffDrawCallCount   { 0 };
    int   OffDrawObjectCount { 0 };

    float PhysicsTimeMs      { 0.0f };
    float RenderTimeMs       { 0.0f };
    float FrameTmeMs         { 0.0f };
    
    int ScreenW              { 0 };
    int ScreenH              { 0 };
};

} // namespace toy
