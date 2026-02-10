#include "Render/GL/GLRenderer.h"
#include "Engine/Core/Application.h"
#include "Render/GL/Shader.h"
#include "Asset/Material/Texture.h"
#include "Render/GL/GLRenderTarget.h"

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
    mWindow    = app->GetSDLWindow();      // 非所有
    mGLContext = app->GetGLContext();   // 非所有

    //---------------------------------------------------------
    // OpenGL コンテキスト属性設定
    //---------------------------------------------------------
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    
    //---------------------------------------------------------
    // OpenGL コンテキスト生成
    //---------------------------------------------------------
    mGLContext = SDL_GL_CreateContext(mWindow);
    if (!mGLContext)
    {
        std::cerr << "Failed to create GL context: " << SDL_GetError() << std::endl;
        return false;
    }

    
    
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

void GLRenderer::Shutdown()
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


PipelineHandle GLRenderer::GetPipelineHandle(const std::string& name)
{
    PipelineHandle h{};
    auto sp = GetShader(name);
    h.ptrGLShader = sp.get();
    h.backend = PipelineBackend::GL;
    return h;
}

void GLRenderer::SetClearColor(const Vector3& color)
{
    mClearColor = color;
    glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);
}


std::shared_ptr<RenderTarget>  GLRenderer::CreateRenderTarget()
{
    return std::make_shared<GLRenderTarget>();
}

} // namespace toy
