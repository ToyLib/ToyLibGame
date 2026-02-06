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

#include "Engine/Render/IRenderer.h"

#include "Engine/Core/Application.h"

// Engine / Render
#include "Engine/Render/LightingManager.h"
#include "Engine/Render/RenderTarget.h"
#include "Engine/Render/Shader.h"

// Graphics
#include "Graphics/VisualComponent.h"

// Asset / Geometry
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"

// Physics / Core
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"

// GL
#include "glad/glad.h"

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
// 初期化／終了処理
//=============================================================
bool IRenderer::Initialize(const Application* app)
{
    mWindow    = app->GetSDLWindow();      // 非所有
    mGLContext = app->GetGLContext();   // 非所有

    // VSync
    SDL_GL_SetSwapInterval(1);

    // 実ピクセルサイズ（HiDPI）
    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSizeInPixels(mWindow, &pixelW, &pixelH);
    mScreenWidth  = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    // DPI scale
    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f) mWindowDisplayScale = 1.0f;

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return false;
    }

    // Shaders
    if (!LoadShaders())
    {
        return false;
    }

    // Common geometry
    CreateSpriteVerts();
    CreateFullScreenQuad();
    CreateSurfaceQuad();

    // Shadow mapping
    if (!InitializeShadowMapping())
    {
        return false;
    }

    // Clear color
    SetClearColor(mClearColor);

    // Size-dependent updates
    OnWindowResized(pixelW, pixelH);

    std::cerr << "[Renderer] GL Init Complete. "
              << "Pixels(" << pixelW << "x" << pixelH << ") "
              << "Scale="  << mWindowDisplayScale
              << std::endl;

    return true;
}

void IRenderer::Shutdown()
{
    // current を保証（できなければ何もしない）
    if (!mWindow || !mGLContext) return;
    if (SDL_GL_MakeCurrent(mWindow, mGLContext) != 0) return;

    // Shadow textures
    for (auto& tex : mShadowMapTexture)
    {
        if (tex)
        {
            tex->Unload();
            tex.reset();
        }
    }

    // FBO
    glDeleteFramebuffers(kShadowCascadeCount, mShadowFBO);
    for (int i = 0; i < kShadowCascadeCount; ++i)
    {
        mShadowFBO[i] = 0;
        mLightSpaceMatrix[i] = Matrix4::Identity;
    }

    // Shared geometry
    mFullScreenQuad.reset();
    mSpriteQuad.reset();
    mSurfaceQuad.reset();
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
// データ解放
//=============================================================
void IRenderer::UnloadData()
{
    mVisualComps.clear();
    if (mSceneRT)
    {
        mSceneRT->Unload();
    }
}

//=============================================================
// ウィンドウサイズ変更時
//=============================================================
void IRenderer::OnWindowResized(int pixelW, int pixelH)
{
    if (pixelW <= 0 || pixelH <= 0) return;

    mScreenWidth  = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    glViewport(0, 0, pixelW, pixelH);

    // SceneRT（ポスト用）
    if (!mSceneRT)
    {
        mSceneRT = std::make_shared<RenderTarget>();
    }
    else
    {
        // 今は作り直し（理想は RenderTarget 側で resize）
        mSceneRT = std::make_shared<RenderTarget>();
    }

    if (mSceneRT && !mSceneRT->Create(pixelW, pixelH))
    {
        std::cerr << "[Renderer] Failed to create/recreate SceneRT\n";
        mSceneRT.reset();
    }

    // DPI 再取得（モニタ跨ぎ対策）
    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f) mWindowDisplayScale = 1.0f;

    // Projection
    mProjectionMatrix = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mPerspectiveFOV),
        mScreenWidth,
        mScreenHeight,
        0.1f,
        10000.0f
    );

    // Sprite 2D ViewProj
    auto it = mShaders.find("Sprite");
    if (it != mShaders.end() && it->second)
    {
        it->second->SetActive();
        Matrix4 viewProj = Matrix4::CreateSimpleViewProj(mScreenWidth, mScreenHeight);
        it->second->SetMatrixUniform("uViewProj", viewProj);
    }
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
bool IRenderer::InitializeShadowMapping()
{
    glGenFramebuffers(kShadowCascadeCount, mShadowFBO);

    for (int i = 0; i < kShadowCascadeCount; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO[i]);

        mShadowMapTexture[i] = std::make_shared<Texture>();
        mShadowMapTexture[i]->CreateShadowMap(mShadowFBOWidth, mShadowFBOHeight);

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D,
            mShadowMapTexture[i]->GetTextureID(),
            0
        );

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "Error: Shadow framebuffer[" << i << "] is not complete!\n";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

//=============================================================
// その他ユーティリティ
//=============================================================
void IRenderer::SetClearColor(const Vector3& color)
{
    mClearColor = color;
    glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);
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

ShaderHandle IRenderer::GetShaderHandle(const std::string& name)
{
    ShaderHandle h{};
    auto sp = GetShader(name);
    h.ptr = sp.get();
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
