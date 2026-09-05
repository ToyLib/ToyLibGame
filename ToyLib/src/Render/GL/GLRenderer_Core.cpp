#include "Render/GL/GLRenderer.h"
#include "Engine/Core/Application.h"
#include "Render/GL/GLShader.h"
#include "Asset/Material/Texture.h"
#include "Render/GL/GLRenderTarget.h"
#include "Render/GL/GLShaderTypes.h"
#include "Render/GL/GLBindingPoints.h"
#include "Render/LightingManager.h"

#include <algorithm>
#include <cstring>
#include <iostream>


namespace toy {

GLRenderer::GLRenderer()
: IRenderer()
{
    
}

GLRenderer::~GLRenderer()
{
    
}

bool GLRenderer::Initialize(const Application* app)
{
    mWindow = app->GetSDLWindow();      // 非所有

    // OpenGL コンテキスト属性は、ウィンドウのピクセルフォーマット確定に
    // 影響するため SDL_CreateWindow より前(Application::Initialize)で設定済み

    //---------------------------------------------------------
    // OpenGL コンテキスト生成
    //---------------------------------------------------------
    mGLContext = SDL_GL_CreateContext(mWindow);
    if (!mGLContext)
    {
        std::cerr << "Failed to create GL context: " << SDL_GetError() << std::endl;
        return false;
    }
    std::cerr << "[Renderer] GL context created." << std::endl;


    // VSync
    SDL_GL_SetSwapInterval(mVSync ? 1 : 0);

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
    
    const GLubyte* deviceName = glGetString(GL_RENDERER);
    mDeviceName = std::string(reinterpret_cast<const char*>(deviceName));
    std::cerr << "GPU: " << mDeviceName << std::endl;

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

    // Scene UBO（フレーム単位のシーンデータ用 Uniform Buffer）
    CreateSceneUBO();

    // Shadow Scene UBO（シャドウ深度パス専用）
    CreateShadowUBO();

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

void GLRenderer::Shutdown()
{
    // current を保証（できなければ何もしない）
    if (!mWindow || !mGLContext) return;
    if (SDL_GL_MakeCurrent(mWindow, mGLContext) != 0) return;

    // ★mSceneCaptureQueue(IRenderer基底クラスのメンバ)はshared_ptr<IRenderTarget>を
    //   保持し得る。GLコンテキスト破棄後に~IRenderer()側でRenderTargetが
    //   破棄されるとGL関数が無効なコンテキストに対して呼ばれてしまうため、
    //   ここで先に空にしておく（VKRenderer::Shutdown()と同様の対策）。
    mSceneCaptureQueue.clear();

    // Shadow textures
    for (auto& tex : mShadowMapTexture)
    {
        if (tex)
        {
            tex->Unload();
            tex.reset();
        }
    }

    // Scene UBO
    if (mSceneUBO)
    {
        glDeleteBuffers(1, &mSceneUBO);
        mSceneUBO = 0;
    }

    // Shadow Scene UBO
    if (mShadowUBO)
    {
        glDeleteBuffers(1, &mShadowUBO);
        mShadowUBO = 0;
    }

    // FBO
    glDeleteFramebuffers(kShadowCascadeCount, mShadowFBO);
    for (int i = 0; i < kShadowCascadeCount; ++i)
    {
        mShadowFBO[i] = 0;
        mLightSpaceMatrix[i] = Matrix4::Identity;
    }

    // Shaders（GLプログラムオブジェクトを解放。コンテキスト破棄より前に行う）
    mShaders.clear();

    // Shared geometry
    mFullScreenQuad.reset();
    mSpriteQuad.reset();
    mSurfaceQuad.reset();

    // GL コンテキストは Initialize() 内で SDL_GL_CreateContext() により生成・所有している
    if (mGLContext)
    {
        SDL_GL_DestroyContext(mGLContext);
        mGLContext = nullptr;
    }
}

//=============================================================
// データ解放
//=============================================================
void GLRenderer::UnloadData()
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
void GLRenderer::OnWindowResized(int pixelW, int pixelH)
{
    if (pixelW <= 0 || pixelH <= 0) return;

    mScreenWidth  = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    glViewport(0, 0, pixelW, pixelH);

    // SceneRT（ポスト用）
    if (!mSceneRT)
    {
        mSceneRT = std::make_shared<GLRenderTarget>();
    }
    else
    {
        // 今は作り直し（理想は RenderTarget 側で resize）
        mSceneRT->Unload();
        mSceneRT = std::make_shared<GLRenderTarget>();
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

bool GLRenderer::InitializeShadowMapping()
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


Matrix4 GLRenderer::GetLightSpaceMatrix(int cascadeIndex) const
{
    if (cascadeIndex < 0 || cascadeIndex >= kShadowCascadeCount) return Matrix4::Identity;
    return mLightSpaceMatrix[cascadeIndex];
}

std::shared_ptr<Texture> GLRenderer::GetShadowMapTexture(int cascadeIndex) const
{
    if (cascadeIndex < 0 || cascadeIndex >= kShadowCascadeCount) return nullptr;
    return mShadowMapTexture[cascadeIndex];
}

PipelineHandle GLRenderer::GetPipelineHandle(const std::string& name)
{
    PipelineHandle h{};
    auto sp = GetShader(name);
    h.ptrGLShader = sp.get();
    h.backend = PipelineBackend::GL;
    return h;
}

// SortBucket_Shadow でシェーダー単位にまとめるためのソートキー
// ptrGLShader が同一 → SetActive 呼び出しを削減できる
uint64_t GLRenderer::GetPipelineSortKey(const PipelineHandle& h) const
{
    return reinterpret_cast<uint64_t>(h.ptrGLShader);
}

void GLRenderer::SetClearColor(const Vector3& color)
{
    mClearColor = color;
    glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);
}


std::shared_ptr<IRenderTarget>  GLRenderer::CreateRenderTarget()
{
    return std::make_shared<GLRenderTarget>();
}

//=============================================================
// Scene UBO
//=============================================================

void GLRenderer::CreateSceneUBO()
{
    glGenBuffers(1, &mSceneUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, mSceneUBO);
    glBufferData(GL_UNIFORM_BUFFER,
                 sizeof(toy::GLSceneUBO),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GLRenderer::UploadSceneUBO()
{
    if (!mSceneUBO) return;

    toy::GLSceneUBO data{};

    //----------------------------------------------------------
    // View-Projection（行優先、v*M 規約）
    //----------------------------------------------------------
    const Matrix4 viewProj = mViewMatrix * mProjectionMatrix;
    std::memcpy(data.viewProj, viewProj.GetAsFloatPtr(), sizeof(data.viewProj));

    //----------------------------------------------------------
    // Camera position（InvView の平行移動成分）
    //----------------------------------------------------------
    const Vector3 camPos = mInvView.GetTranslation();
    data.cameraAndSun[0] = camPos.x;
    data.cameraAndSun[1] = camPos.y;
    data.cameraAndSun[2] = camPos.z;
    // data.cameraAndSun[3] = sunIntensity（下でライティングデータから設定）

    //----------------------------------------------------------
    // Lighting / Fog（LightingManager から取得）
    //----------------------------------------------------------
    if (mLightingManager)
    {
        const SceneLightData d = mLightingManager->BuildLightData(mViewMatrix);

        data.cameraAndSun[3] = d.sunIntensity;

        data.ambientLight[0] = d.ambientColor.x;
        data.ambientLight[1] = d.ambientColor.y;
        data.ambientLight[2] = d.ambientColor.z;

        data.dirDirection[0] = d.dirDirection.x;
        data.dirDirection[1] = d.dirDirection.y;
        data.dirDirection[2] = d.dirDirection.z;

        data.dirDiffuse[0] = d.dirDiffuse.x;
        data.dirDiffuse[1] = d.dirDiffuse.y;
        data.dirDiffuse[2] = d.dirDiffuse.z;

        data.dirSpecular[0] = d.dirSpecular.x;
        data.dirSpecular[1] = d.dirSpecular.y;
        data.dirSpecular[2] = d.dirSpecular.z;

        data.numPointLights = d.numPointLights;

        const int plCount = std::min(d.numPointLights, 8);
        for (int i = 0; i < plCount; ++i)
        {
            const PointLightData& pl = d.pointLights[i];

            data.plPosRadius[i][0] = pl.position.x;
            data.plPosRadius[i][1] = pl.position.y;
            data.plPosRadius[i][2] = pl.position.z;
            data.plPosRadius[i][3] = pl.radius;

            data.plColorIntensity[i][0] = pl.color.x;
            data.plColorIntensity[i][1] = pl.color.y;
            data.plColorIntensity[i][2] = pl.color.z;
            data.plColorIntensity[i][3] = pl.intensity;

            data.plAtten[i][0] = pl.constant;
            data.plAtten[i][1] = pl.linear;
            data.plAtten[i][2] = pl.quadratic;
        }

        data.fogColor[0] = d.fogColor.x;
        data.fogColor[1] = d.fogColor.y;
        data.fogColor[2] = d.fogColor.z;

        data.fogParams[0] = d.fogMinDist;
        data.fogParams[1] = d.fogMaxDist;
    }

    //----------------------------------------------------------
    // Shadow matrices（DrawShadowPass() で計算済み）
    //----------------------------------------------------------
    std::memcpy(data.lightViewProj0,
                mLightSpaceMatrix[0].GetAsFloatPtr(),
                sizeof(data.lightViewProj0));
    std::memcpy(data.lightViewProj1,
                mLightSpaceMatrix[1].GetAsFloatPtr(),
                sizeof(data.lightViewProj1));

    //----------------------------------------------------------
    // Shadow params
    //----------------------------------------------------------
    data.shadowParams[0] = GetCascadeSplit0();
    data.shadowParams[1] = GetCascadeBlend();
    data.shadowParams[2] = GetShadowBias();
    data.shadowFlags[0]  = static_cast<int>(GetEnableShadow());

    //----------------------------------------------------------
    // GPU へ転送してバインドポイントに固定
    //----------------------------------------------------------
    glBindBuffer(GL_UNIFORM_BUFFER, mSceneUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(toy::GLSceneUBO), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, toy::gl::kSceneUBOBinding, mSceneUBO);
}

//=============================================================
// Shadow Scene UBO（シャドウ深度パス専用。VK の ShadowSceneUBO 相当）
//=============================================================

void GLRenderer::CreateShadowUBO()
{
    glGenBuffers(1, &mShadowUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, mShadowUBO);
    glBufferData(GL_UNIFORM_BUFFER,
                 sizeof(toy::GLShadowSceneUBO),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GLRenderer::UploadShadowUBO(const Matrix4& lightVP)
{
    if (!mShadowUBO) return;

    toy::GLShadowSceneUBO data{};
    std::memcpy(data.lightVP, lightVP.GetAsFloatPtr(), sizeof(data.lightVP));

    glBindBuffer(GL_UNIFORM_BUFFER, mShadowUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(toy::GLShadowSceneUBO), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, toy::gl::kShadowUBOBinding, mShadowUBO);
}

} // namespace toy
