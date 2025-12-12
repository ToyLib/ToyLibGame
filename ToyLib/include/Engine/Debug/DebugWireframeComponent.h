#pragma once

#include "Graphics/Effect/WireframeComponent.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Runtime/InputSystem.h"

namespace toy {

class DebugWireframeComponent : public WireframeComponent
{
public:
    DebugWireframeComponent(Actor* owner, int drawOrder = 1000,
                            VisualLayer layer = VisualLayer::Effect3D)
    : WireframeComponent(owner, drawOrder, layer)
    {}
    
    void Draw() override
    {
        auto* app = GetOwner()->GetApp();
        if (!app) return;
        
        // DebugMode=false のときはそもそも生成されない前提でも、
        // 念のためランタイム表示フラグで抑制できるようにしておく
        if (!app->GetRenderer()->GetDebuWireVisible())
        {
            return;
        }
        WireframeComponent::Draw();
    }

};

} // namespace toy
