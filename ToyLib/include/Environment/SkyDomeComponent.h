#pragma once

#include "Graphics/VisualComponent.h"
#include <memory>

namespace toy {

class VertexArray;
class Shader;
class LightingManager;

class SkyDomeComponent : public VisualComponent
{
public:
    SkyDomeComponent(class Actor* owner, int drawOrder = 0, VisualLayer layer = VisualLayer::Sky);
    ~SkyDomeComponent() override;          // ★cppで定義（=defaultでもOKだがcpp側）

    void Update(float deltaTime) override;
    void Draw() override {}
    void GatherRenderItems(class RenderQueue& outQueue) override;

    void SetLightingManager(std::shared_ptr<LightingManager> mgr)
    {
        mLightingManager = std::move(mgr);
    }

protected:
    // ★宣言だけ（定義はcppへ）
    void SetSkyGeometry(std::unique_ptr<VertexArray> vao);
    void SetSkyShader(std::shared_ptr<Shader> shader);

    VertexArray* GetSkyVAO() const { return mSkyVAO.get(); }
    Shader*      GetShader() const { return mShader.get(); }

protected:
    std::unique_ptr<VertexArray>      mSkyVAO;
    std::shared_ptr<LightingManager>  mLightingManager;
    std::shared_ptr<Shader>           mShader;

    float mSkyScale = 200.0f;
};

} // namespace toy
