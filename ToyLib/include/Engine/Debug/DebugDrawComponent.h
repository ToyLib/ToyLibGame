#pragma once

#include "Graphics/VisualComponent.h"
#include <memory>

namespace toy
{

class DebugDrawComponent : public VisualComponent
{
public:
    DebugDrawComponent(Actor* owner,
                       int drawOrder = 9999,
                       VisualLayer layer = VisualLayer::Object3D);

    void GatherRenderItems(class RenderQueue& q) override;
    void PreDraw();
    
    void ProcessInput(const struct InputState& state) override;

private:
    std::shared_ptr<class VertexArray> mVertexArray;
};

} // namespace toy::kit
