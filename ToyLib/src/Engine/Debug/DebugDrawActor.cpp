#include "Engine/Debug/DebugDrawActor.h"
#include "Engine/Debug/DebugDrawComponent.h"

namespace toy
{

DebugDrawActor::DebugDrawActor(Application* app)
    : Actor(app)
{
    CreateComponent<DebugDrawComponent>(9999, VisualLayer::Object3D);
}

} // namespace toy
