//==============================================================================
// IRenderer_Core.cpp
//  - ctor/dtor
//  - Initialize/Shutdown
//  - Camera stack / SceneCapture request
//  - VisualComponent register
//  - Common geometry (Sprite/FullScreen/Surface)
//  - Window resize / UI scale
//  - Shadow init
//  - Small utilities (handles, clear color, etc.)
//==============================================================================

#include "Render/IRenderer.h"

#include "Engine/Core/Application.h"

// Engine / Render
#include "Render/LightingManager.h"
#include "Render/RenderTarget.h"

// Graphics
#include "Graphics/VisualComponent.h"

// Asset / Geometry
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"

// Physics / Core
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"

// Std
#include <algorithm>
#include <iostream>
#include <string>

namespace toy {

//=============================================================
// コンストラクタ／デストラクタ
//=============================================================
IRenderer::IRenderer()
{
    mLightingManager = std::make_shared<LightingManager>();
    LoadSettings("ToyLib/Settings/Renderer_Settings.json");
}

IRenderer::~IRenderer()
{
    // 実処理は Shutdown() 側で行う前提
}


//=============================================================
// カメラ切り替え
//=============================================================
void IRenderer::PushCameraState()
{
    CameraState s{};
    s.view     = mViewMatrix;
    s.proj     = mProjectionMatrix;
    s.viewProj = mViewMatrix * mProjectionMatrix;
    s.invView  = mInvView;
    mCameraStack.push_back(s);
}

void IRenderer::SetCameraState(const CameraState& s)
{
    mViewMatrix       = s.view;
    mProjectionMatrix = s.proj;
    mInvView          = s.invView;
}

void IRenderer::PopCameraState()
{
    if (mCameraStack.empty()) return;
    const CameraState s = mCameraStack.back();
    mCameraStack.pop_back();
    SetCameraState(s);
}

//=============================================================
// シーンキャプチャーリクエスト
//=============================================================
void IRenderer::RequestSceneCapture(const SceneCaptureRequest& req)
{
    if (!req.rt) return;
    mSceneCaptureQueue.push_back(req);
}

//=============================================================
// VisualComponent 管理
//=============================================================
void IRenderer::AddVisualComp(VisualComponent* comp)
{
    auto iter = mVisualComps.begin();
    for (; iter != mVisualComps.end(); ++iter)
    {
        if (comp->GetDrawOrder() < (*iter)->GetDrawOrder())
        {
            break;
        }
    }
    mVisualComps.insert(iter, comp);
}

void IRenderer::RemoveVisualComp(VisualComponent* comp)
{
    auto iter = std::find(mVisualComps.begin(), mVisualComps.end(), comp);
    if (iter != mVisualComps.end())
    {
        mVisualComps.erase(iter);
    }
}

//=============================================================
// 共通ジオメトリ
//=============================================================
void IRenderer::CreateSpriteVerts()
{
    const float vertices[] =
    {
        -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };

    const unsigned int indices[] =
    {
        2, 1, 0,
        0, 3, 2
    };

    mSpriteQuad = std::make_shared<VertexArray>(
        (float*)vertices, 4,
        (unsigned int*)indices, 6
    );
}

void IRenderer::CreateFullScreenQuad()
{
    float quadVerts[] =
    {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };
    unsigned int quadIndices[] =
    {
        0, 1, 2,
        2, 3, 0
    };

    mFullScreenQuad = std::make_shared<VertexArray>(
        quadVerts, 4, quadIndices, 6, true
    );
}

void IRenderer::CreateSurfaceQuad()
{
    static const float pos[] =
    {
        -0.5f,  0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f
    };

    static const float norm[] =
    {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };

    static const float uv[] =
    {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    static const unsigned int idx[] =
    {
        0, 2, 1,
        0, 3, 2
    };

    mSurfaceQuad = std::make_shared<VertexArray>(
        /*numVerts*/   4,
        /*verts*/      pos,
        /*norms*/      norm,
        /*uvs*/        uv,
        /*numIndices*/ 6,
        /*indices*/    idx
    );
}




//=============================================================
// UI / Virtual 解像度関連
//=============================================================
void IRenderer::SetVirtualResolution(float w, float h)
{
    mVirtualWidth  = w;
    mVirtualHeight = h;
}

UIScaleInfo IRenderer::GetUIScaleInfo() const
{
    UIScaleInfo info{};
    info.screenW = mScreenWidth;
    info.screenH = mScreenHeight;

    info.virtualW = (mVirtualWidth  > 0.0f) ? mVirtualWidth  : mScreenWidth;
    info.virtualH = (mVirtualHeight > 0.0f) ? mVirtualHeight : mScreenHeight;

    if (info.virtualW <= 0.0f) info.virtualW = 1.0f;
    if (info.virtualH <= 0.0f) info.virtualH = 1.0f;

    info.scaleX = info.screenW / info.virtualW;
    info.scaleY = info.screenH / info.virtualH;
    info.scale  = (info.scaleX < info.scaleY) ? info.scaleX : info.scaleY;

    return info;
}

//=============================================================
// シャドウマッピング
//=============================================================

//=============================================================
// その他ユーティリティ
//=============================================================
void IRenderer::SetClearColor(const Vector3& color)
{
    mClearColor = color;
}

GeometryHandle IRenderer::GetSpriteQuadHandle() const
{
    GeometryHandle h{};
    h.ptr = mSpriteQuad.get();
    return h;
}

GeometryHandle IRenderer::GetSurfaceQuadHandle() const
{
    GeometryHandle h{};
    h.ptr = mSurfaceQuad.get();
    return h;
}


TextureHandle IRenderer::ToHandle(const std::shared_ptr<Texture>& tex) const
{
    TextureHandle h{};
    h.ptr = tex.get();
    return h;
}

MaterialHandle IRenderer::ToHandle(const std::shared_ptr<Material>& mat) const
{
    MaterialHandle h{};
    h.ptr = mat.get();
    return h;
}

} // namespace toy
