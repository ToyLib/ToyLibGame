#pragma once

#include "Graphics/VisualComponent.h"
#include <memory>

namespace toy
{
class VertexArray;
}

namespace toy::kit
{

class DebugDrawComponent : public toy::VisualComponent
{
public:
    DebugDrawComponent(toy::Actor* owner,
                       int drawOrder = 9999,
                       toy::VisualLayer layer = toy::VisualLayer::Object3D);

    void GatherRenderItems(toy::RenderQueue& q) override;
    void PreDraw();

private:
    std::shared_ptr<toy::VertexArray> mVertexArray;
};

} // namespace toy::kit
