#include "Engine/Debug/DebugWireframeComponent.h"

#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"

namespace toy {

DebugWireframeComponent::DebugWireframeComponent(
    Actor* owner,
    int drawOrder,
    VisualLayer layer
)
    : WireframeComponent(owner, drawOrder, layer)
{
}

void DebugWireframeComponent::GatherRenderItems(RenderQueue& q)
{
    auto* app = GetOwner()->GetApp();
    if (!app) return;

    if (!app->GetVisibleDebugWire())
        return;

    // 実体は Wireframe の積み方そのまま
    WireframeComponent::GatherRenderItems(q);
}

} // namespace toy
