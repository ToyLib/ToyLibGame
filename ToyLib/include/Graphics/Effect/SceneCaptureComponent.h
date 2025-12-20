#pragma once
#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"
#include <memory>

namespace toy {


class SceneCaptureComponent : public Component
{
public:
    struct Desc
    {
        int   width  = 512;
        int   height = 512;

        bool  enabled = true;
        bool  drawUI  = false;     // まずは false 推奨
        float fov     = 45.0f;
        float updateHz = 60.0f;    // 0なら毎フレーム
    };

    SceneCaptureComponent(class Actor* owner);

    void Init(const Desc& desc);
    void Update(float deltaTime) override;

    std::shared_ptr<class Texture> GetColorTexture() const;

    // キャプチャ用カメラ（Actorの向きで作る想定。必要なら外部からもセット可）
    void SetViewProj(const Matrix4& view, const Matrix4& proj);

private:
    void Capture();

    Desc mDesc{};

    std::shared_ptr<class RenderTarget> mRT;

    Matrix4 mView;
    Matrix4 mProj;

    float mAcc;
};

} // namespace toy
