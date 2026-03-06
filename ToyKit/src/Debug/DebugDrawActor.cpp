#include "Debug/DebugDrawActor.h"
#include "Debug/DebugDrawComponent.h"

namespace toy::kit
{

DebugDrawActor::DebugDrawActor(toy::Application* app)
    : Actor(app)
{
    CreateComponent<DebugDrawComponent>(9999, toy::VisualLayer::Object3D);
}

} // namespace toy::kit
