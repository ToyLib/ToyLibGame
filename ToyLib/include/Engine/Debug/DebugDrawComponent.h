#pragma once

#include "Graphics/VisualComponent.h"
#include <memory>

namespace toy
{
class VertexArray;

class DebugDrawComponent : public VisualComponent
{
public:
    DebugDrawComponent(Actor* owner,
                       int drawOrder = 9999,
                       VisualLayer layer = VisualLayer::Object3D);

    void GatherRenderItems(toy::RenderQueue& q) override;
    void PreDraw();
    
    void ProcessInput(const struct InputState& state);

private:
    std::shared_ptr<VertexArray> mVertexArray;
};

} // namespace toy::kit
