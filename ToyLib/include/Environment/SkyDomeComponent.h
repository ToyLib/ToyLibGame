#pragma once

#include "Graphics/VisualComponent.h"
#include <memory>

namespace toy {

class VertexArray;
class LightingManager;

class SkyDomeComponent : public VisualComponent
{
public:
    SkyDomeComponent(class Actor* owner, int drawOrder = 0, VisualLayer layer = VisualLayer::Sky);
    ~SkyDomeComponent() override;          // ★cppで定義（=defaultでもOKだがcpp側）

    void Update(float deltaTime) override;
    void GatherRenderItems(class RenderQueue& outQueue) override;

    void SetLightingManager(std::shared_ptr<LightingManager> mgr)
    {
        mLightingManager = std::move(mgr);
    }


protected:
    std::unique_ptr<VertexArray>      mSkyVAO;
    std::shared_ptr<LightingManager>  mLightingManager;

    float mSkyScale { 200.0f };
};

} // namespace toy
