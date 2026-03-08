#include "Engine/Debug/DebugDrawActor.h"
#include "Engine/Debug/DebugDrawComponent.h"

namespace toy
{

DebugDrawActor::DebugDrawActor(toy::Application* app)
    : Actor(app)
{
    CreateComponent<DebugDrawComponent>(9999, toy::VisualLayer::Object3D);
}

} // namespace toy
