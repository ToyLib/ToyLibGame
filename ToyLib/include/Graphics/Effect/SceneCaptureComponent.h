#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h" // Matrix4, Vector3

#include <memory>

namespace toy {

//==============================================================================
// キャプチャーモード
//==============================================================================
enum class CaptureMode
{
    Fixed,   // Actor基準の固定カメラ
    Mirror,  // 鏡：メインカメラを反射
    Water    // 水面：
};

//==============================================================================
// レンダリング先のサーフェス情報
//==============================================================================
struct SurfaceInfo
{
    float scWidth  = 1.0f;  // スクリーン上にスケールされる実サイズ
    float scHeight = 1.0f;
};

// Renderer 側にある想定（前方宣言）
struct SceneCaptureRequest;

//==============================================================================
// SceneCaptureComponent
//------------------------------------------------------------------------------
class SceneCaptureComponent : public Component
{
public:
    //==========================================================================
    // Desc
    //==========================================================================
    struct Desc
    {
        int   width    = 512;
        int   height   = 512;

        bool  enabled  = true;
        bool  drawUI   = false;

        float fov      = 45.0f;
        float updateHz = 60.0f; // 0なら毎フレーム
    };

    //--------------------------------------------------------------------------
    // ctor / setup
    //--------------------------------------------------------------------------
    explicit SceneCaptureComponent(class Actor* owner);

    void Init(const Desc& desc);

    //--------------------------------------------------------------------------
    // Component
    //--------------------------------------------------------------------------
    void Update(float deltaTime) override;

    //--------------------------------------------------------------------------
    // Outputs
    //--------------------------------------------------------------------------
    std::shared_ptr<class Texture> GetColorTexture() const;

    //--------------------------------------------------------------------------
    // Capture camera
    //--------------------------------------------------------------------------
    void SetViewProj(const Matrix4& view, const Matrix4& proj);

    void SetCaptureMode(CaptureMode mode) { mCaptureMode = mode; }
    void SetSurfaceInfo(const SurfaceInfo& info) { mSurfaceInfo = info; }

private:
    //--------------------------------------------------------------------------
    // Internal
    //--------------------------------------------------------------------------
    void Capture();

    void BuildFixedView();
    void BuildMirrorView();
    void BuildWaterView();
private:
    Desc mDesc{};

    std::shared_ptr<class RenderTarget> mRT;

    Matrix4 mView { Matrix4::Identity };
    Matrix4 mProj { Matrix4::Identity };

    float mAcc { 0.0f };

    CaptureMode mCaptureMode { CaptureMode::Fixed };
    SurfaceInfo mSurfaceInfo {};
};

} // namespace toy
