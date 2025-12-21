#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h" // Matrix4 など

#include <memory>

namespace toy {



//==============================================================================
// キャプチャーモード
//==============================================================================
enum class CaptureMode
{
    Fixed,   // Actor基準の固定カメラ（今の実装）
    Mirror,  // 鏡：メインカメラを反射
};

//==============================================================================
// レンダリング先のサーフェス情報
//==============================================================================
struct SurfaceInfo
{
    float scWidth     = 1.0f;    // スクリーン上にスケールされる実サイズ
    float scHeight    = 1.0f;
};


//==============================================================================
// SceneCaptureComponent
//------------------------------------------------------------------------------
// ・指定した View/Proj で RenderTarget に描画するキャプチャ用コンポーネント
// ・更新頻度（Hz）や UI 描画の有無を Desc で制御
//==============================================================================
class SceneCaptureComponent : public Component
{
public:
    //==========================================================================
    // Desc
    //--------------------------------------------------------------------------
    // ・キャプチャ設定
    // ・Init() でまとめて渡す想定
    //==========================================================================
    struct Desc
    {
        int   width    = 512;   // キャプチャ解像度（幅）
        int   height   = 512;   // キャプチャ解像度（高さ）

        bool  enabled  = true;  // キャプチャ有効/無効
        bool  drawUI   = false; // UI を描くか（まずは false 推奨）

        float fov      = 45.0f;
        float updateHz = 60.0f; // 更新頻度。0 なら毎フレーム更新
        
    };

    //--------------------------------------------------------------------------
    // ctor / setup
    //--------------------------------------------------------------------------
    SceneCaptureComponent(class Actor* owner);

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
    // キャプチャ用の View/Proj をセット（Actor の向きで生成する想定だが外部指定も可）
    void SetViewProj(const Matrix4& view, const Matrix4& proj);
    
    void SetCaptureMode(const CaptureMode mode) { mCaptureMode = mode; }
    
    void SetSurfaceInfo(const SurfaceInfo info) { mSurfaceInfo = info; }

private:
    //--------------------------------------------------------------------------
    // Internal
    //--------------------------------------------------------------------------
    void Capture();

    void BuildFixedView();
    void BuildMirrorView();
    
private:
    // 設定
    Desc mDesc{};

    // 出力先（カラー等を持つ RT）
    std::shared_ptr<class RenderTarget> mRT;

    // キャプチャ用カメラ行列
    Matrix4 mView;
    Matrix4 mProj;

    // updateHz 用の蓄積時間
    float mAcc;
    
    CaptureMode mCaptureMode;
    
    SurfaceInfo mSurfaceInfo;
};

} // namespace toy
