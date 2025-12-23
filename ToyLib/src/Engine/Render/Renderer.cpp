//==============================================================================
// Renderer.cpp
//  - OpenGL 描画の初期化/終了
//  - 描画パス（通常/RenderTarget）
//  - VisualComponent 管理 & レイヤー描画（フラスタムカリング）
//  - 共通ジオメトリ生成（Sprite/FullScreen/Surface）
//  - シャドウマッピング（カスケード）
//  - シェーダーロード
//  - テキストテクスチャ生成（改行対応）
//  - WorldToScreen（画面投影）
//==============================================================================

#include "Engine/Render/Renderer.h"

//==============================================================================
// Engine / Render
//==============================================================================
#include "Engine/Render/LightingManager.h"
#include "Engine/Render/RenderTarget.h"
#include "Engine/Render/Shader.h"

//==============================================================================
// Graphics
//==============================================================================
#include "Graphics/VisualComponent.h"
#include "Graphics/Effect/ParticleComponent.h"
#include "Graphics/Effect/RenderSurfaceComponent.h"
#include "Graphics/Mesh/MeshComponent.h"
#include "Graphics/Mesh/SkeletalMeshComponent.h"
#include "Graphics/Sprite/BillboardComponent.h"
#include "Graphics/Sprite/SpriteComponent.h"

//==============================================================================
// Asset
//==============================================================================
#include "Asset/Font/TextFont.h"
#include "Asset/Geometry/Mesh.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"

//==============================================================================
// Environment / Physics / Core
//==============================================================================
#include "Environment/SkyDomeComponent.h"
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"

//==============================================================================
// Utils
//==============================================================================
#include "Utils/FrustumUtil.h"

//==============================================================================
// GL
//==============================================================================
#include "glad/glad.h"

//==============================================================================
// Standard Library
//==============================================================================
#include <algorithm>
#include <iostream>
#include <string>

namespace toy {

//=============================================================
// コンストラクタ／デストラクタ
//=============================================================

Renderer::Renderer()
    : mScreenWidth(0.0f)
    , mScreenHeight(0.0f)
    , mVirtualWidth(0.0f)
    , mVirtualHeight(0.0f)
    , mPerspectiveFOV(45.0f)
    , mIsDebugMode(false)
    , mIsDebugWireVisible(false)
    , mClearColor(Vector3(0.2f, 0.5f, 0.8f))
    , mWireColor(Vector3(1.0f, 1.0f, 1.0f))
    , mShadowNear(10.f)
    , mShadowFar(100)
    , mShadowOrthoWidth(100.f)
    , mShadowOrthoHeight(100.f)
    , mShadowFBOWidth(4096)
    , mShadowFBOHeight(4096)
    , mWindow(nullptr)
    , mGLContext(nullptr)
    , mShaderPath("ToyLib/Shaders/")
//    , mDrawObjectCount(0)
//    , mDrawCallCount(0)
    , mSkyDomeComp(nullptr)
    , mLightSpaceMatrix(Matrix4::Identity)
    , mWindowDisplayScale(1.0f)
    , mCascadeSplit0(25.0f)
    , mCascadeBlend(6.0f)
{
    // ライティング管理クラス（Directional/Point など）
    mLightingManager = std::make_shared<LightingManager>();

    // Renderer の初期設定（タイトル/解像度など）を外部ファイルから読み込む
    // 例: ToyLib/Settings/Renderer_Settings.json
    LoadSettings("ToyLib/Settings/Renderer_Settings.json");
}

Renderer::~Renderer()
{
    // 実処理は Shutdown() 側で行う前提
}


//=============================================================
// 初期化／終了処理
//=============================================================

bool Renderer::Initialize(SDL_Window* window)
{
    // SDLウィンドウを保持
    mWindow = window;

    //---------------------------------------------------------
    // OpenGL コンテキスト属性設定
    //---------------------------------------------------------
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
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

    //---------------------------------------------------------
    // 垂直同期（VSync）
    //---------------------------------------------------------
    SDL_GL_SetSwapInterval(1);

    //---------------------------------------------------------
    // ウィンドウの「実ピクセルサイズ」を取得（HiDPI 対応）
    //---------------------------------------------------------
    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSizeInPixels(mWindow, &pixelW, &pixelH);

    // 描画に使うスクリーンサイズは「実ピクセル」で管理
    mScreenWidth  = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    //---------------------------------------------------------
    // このウィンドウに対する DPI スケール
    //---------------------------------------------------------
    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f)
    {
        mWindowDisplayScale = 1.0f;
    }

    //---------------------------------------------------------
    // GLAD 初期化
    //---------------------------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return false;
    }

    //---------------------------------------------------------
    // シェーダーのロード
    //---------------------------------------------------------
    if (!LoadShaders())
    {
        return false;
    }

    //---------------------------------------------------------
    // 各種描画用 VAO 準備
    //---------------------------------------------------------
    CreateSpriteVerts();
    CreateFullScreenQuad();
    CreateSurfaceQuad();

    //---------------------------------------------------------
    // シャドウマッピング初期化
    //---------------------------------------------------------
    if (!InitializeShadowMapping())
    {
        return false;
    }

    //---------------------------------------------------------
    // クリアカラーの初期設定
    //---------------------------------------------------------
    SetClearColor(mClearColor);
    mSkyDomeComp = nullptr;

    //---------------------------------------------------------
    // ビューポート＆射影行列など、サイズ依存の状態をまとめて更新
    //---------------------------------------------------------
    OnWindowResized(pixelW, pixelH);

    std::cout << "[Renderer] GL Init Complete. "
              << "Pixels("  << pixelW   << "x" << pixelH   << ") "
              << "Scale="   << mWindowDisplayScale
              << std::endl;

    return true;
}

void Renderer::Shutdown()
{
    // シャドウ用 FBO / テクスチャを破棄
    glDeleteFramebuffers(kShadowCascadeCount, mShadowFBO);
    for (int i = 0; i < kShadowCascadeCount; ++i)
    {
        mShadowFBO[i] = 0;
        mShadowMapTexture[i].reset();
        mLightSpaceMatrix[i] = Matrix4::Identity;
    }

    // GL Context 破棄
    if (mGLContext)
    {
        SDL_GL_DestroyContext(mGLContext);
        mGLContext = nullptr;
    }
}


//=============================================================
// メイン描画パス
//=============================================================

void Renderer::Draw()
{
    // Debug 用カウンタリセット
    ResetDebugCounter();

    ChangeDebugRTT();
    FlushSceneCaptures();

    ChangeDebugOnScreen();
    DrawPass(true);
    // バッファ入れ替え
    SDL_GL_SwapWindow(mWindow);
}

void Renderer::DrawPass(bool drawUI)
{
    // カラーバッファ／デプスバッファ初期化
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1) ライト視点でのシャドウマップ描画
    RenderShadowMap();

    // 2) 通常描画パス
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // スカイドーム（背景）
    DrawSky();

    // レイヤー別描画（奥から順に）
    DrawVisualLayer(VisualLayer::Background2D);
    DrawVisualLayer(VisualLayer::Object3D);
    DrawVisualLayer(VisualLayer::Effect3D);
    DrawVisualLayer(VisualLayer::OverlayScreen);
    if (drawUI)
    {
        DrawVisualLayer(VisualLayer::UI);
    }
}

void Renderer::DrawToRenderTarget(std::shared_ptr<RenderTarget> rt,
                                  const Matrix4& view,
                                  const Matrix4& proj,
                                  bool drawUI)
{
    if (!rt)
    {
        return;
    }

    // 退避（描画先/カメラ/Viewport）
    const Matrix4 prevView = mViewMatrix;
    const Matrix4 prevProj = mProjectionMatrix;

    GLint prevVP[4];
    glGetIntegerv(GL_VIEWPORT, prevVP);

    // 出力先をRTへ
    rt->Bind();

    // カメラ差し替え
    mViewMatrix       = view;
    mProjectionMatrix = proj;

    auto skipTex = rt->GetColorTexture();

    //DrawPass(drawUI);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 影は一旦OFF推奨（必要なら前に話したFBO/VP退避復帰を入れて）
    RenderShadowMap();

    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawSky();
    DrawVisualLayer(VisualLayer::Background2D,  skipTex);
    DrawVisualLayer(VisualLayer::Object3D,      skipTex);
    DrawVisualLayer(VisualLayer::Effect3D,      skipTex);
    //DrawVisualLayer(VisualLayer::OverlayScreen, skipTex);
    if (drawUI)
    {
        DrawVisualLayer(VisualLayer::UI, skipTex);
    }

    // 戻す
    mViewMatrix       = prevView;
    mProjectionMatrix = prevProj;

    RenderTarget::Unbind();
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
    //glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);
}
void Renderer::FlushSceneCaptures()
{
    
    for (const auto& req : mSceneCaptureQueue)
    {
        DrawToRenderTarget(
            req.rt,
            req.view,
            req.proj,
            req.drawUI
        );
    }

    mSceneCaptureQueue.clear();
}

//=============================================================
// SkyDome
//=============================================================

void Renderer::DrawSky()
{
    if (!mSkyDomeComp)
    {
        return;
    }
    mSkyDomeComp->Draw();
}


//=============================================================
// VisualComponent 管理
//=============================================================

void Renderer::AddVisualComp(VisualComponent* comp)
{
    // DrawOrder 昇順で挿入
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

void Renderer::RemoveVisualComp(VisualComponent* comp)
{
    auto iter = std::find(mVisualComps.begin(), mVisualComps.end(), comp);
    if (iter != mVisualComps.end())
    {
        mVisualComps.erase(iter);
    }
}


//=============================================================
// レイヤー描画＆フラスタムカリング
//=============================================================

void Renderer::DrawVisualLayer(VisualLayer layer,
                               const std::shared_ptr<Texture>& skipTex)
{
    // 3Dレイヤー判定（Object3D/Effect3D のみカリング対象）
    bool is3DLayer =
        (layer == VisualLayer::Object3D ||
         layer == VisualLayer::Effect3D);

    // 3Dレイヤーのみ、VP からフラスタムを構築
    Frustum frustum;
    if (is3DLayer)
    {
        Matrix4 vp = mViewMatrix * mProjectionMatrix;
        frustum = BuildFrustumFromMatrix(vp);
    }

    // レイヤーごとの depth 設定（既存のまま）
    if (layer == VisualLayer::UI || layer == VisualLayer::Background2D)
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    else if (layer == VisualLayer::Effect3D)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    else
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    for (auto& comp : mVisualComps)
    {
        if (!comp->IsVisible() || comp->GetLayer() != layer)
        {
            continue;
        }

        // RenderTarget への描画時に「自分自身のRTテクスチャ」を描かない用
        if (skipTex)
        {
            if (comp->GetTexture() == skipTex)
            {
                continue;
            }
        }

        // 3Dレイヤーのみフラスタムカリング（既存のまま）
        if (is3DLayer)
        {
            Actor* owner = comp->GetOwner();
            if (owner)
            {
                auto bv = owner->GetComponent<BoundingVolumeComponent>();
                if (bv)
                {
                    Cube aabb = bv->GetWorldAABB();
                    if (!FrustumIntersectsAABB(frustum, aabb))
                    {
                        continue;
                    }
                }
            }
        }

        comp->Draw();
    }

    // 後段に影響しないよう戻す
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}


//=============================================================
// シーンキャプチャーリクエスト
//=============================================================

void Renderer::RequestSceneCapture(const SceneCaptureRequest& req)
{
    mSceneCaptureQueue.emplace_back(req);
}

//=============================================================
// 共通ジオメトリ（スプライト／フルスクリーン）
//=============================================================

void Renderer::CreateSpriteVerts()
{
    // スプライト用四角ポリゴン（ローカル [-0.5, 0.5] の正方形）
    const float vertices[] =
    {
        -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // top left
         0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // top right
         0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, // bottom right
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // bottom left
    };

    const unsigned int indices[] =
    {
        2, 1, 0,
        0, 3, 2
    };

    mSpriteVerts = std::make_shared<VertexArray>(
        (float*)vertices, 4,
        (unsigned int*)indices, 6
    );
}

void Renderer::CreateFullScreenQuad()
{
    // フルスクリーンクアッド（PostEffect / 天候オーバーレイなど）
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

    // 2D 頂点のみの簡易 VAO（isScreenQuad = true の想定）
    mFullScreenQuad = std::make_shared<VertexArray>(
        quadVerts, 4, quadIndices, 6, true
    );
}

void Renderer::CreateSurfaceQuad()
{
    //==========================================================
    // 3D置物用の 1x1 Quad（ローカル [-0.5..0.5], Z=0）
    //  - pos/norm/uv を別配列で渡す（VertexArray 通常メッシュ用）
    //  - 物理用ポリゴン生成も pos を正しく参照できる
    //==========================================================

    // ---- Position (vec3 * 4) ----
    static const float pos[] =
    {
        -0.5f,  0.5f, 0.0f,   // 0: top left
         0.5f,  0.5f, 0.0f,   // 1: top right
         0.5f, -0.5f, 0.0f,   // 2: bottom right
        -0.5f, -0.5f, 0.0f    // 3: bottom left
    };

    // ---- Normal (vec3 * 4) ----
    // 表が +Z を向く想定。RenderSurface ではライティングしないが、
    // 後でフレネル等を入れる可能性もあるので入れておく。
    static const float norm[] =
    {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };

    // ---- UV (vec2 * 4) ----
    static const float uv[] =
    {
        0.0f, 0.0f,   // 0
        1.0f, 0.0f,   // 1
        1.0f, 1.0f,   // 2
        0.0f, 1.0f    // 3
    };

    // ---- Indices (6) ----
    // CCW を維持（表面が +Z 方向に見える）
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

void Renderer::UnloadData()
{
    // VisualComponent の登録だけをクリア
    // 実際の Mesh/Texture などのリソースは AssetManager 側で管理する想定
    mVisualComps.clear();
}


//=============================================================
// ウィンドウサイズ変更時
//=============================================================

void Renderer::OnWindowResized(int pixelW, int pixelH)
{
    if (pixelW <= 0 || pixelH <= 0)
    {
        return;
    }

    mScreenWidth  = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    glViewport(0, 0, pixelW, pixelH);

    // DPI スケールもここで取り直しておくと、モニタ跨ぎ時も安全
    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f)
    {
        mWindowDisplayScale = 1.0f;
    }

    // ここは Matrix4::CreatePerspectiveFOV のシグネチャに合わせて
    mProjectionMatrix = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mPerspectiveFOV),
        mScreenWidth,
        mScreenHeight,
        0.1f,
        10000.0f
    );

    // Sprite シェーダーに 2D 用 ViewProj を再設定
    auto it = mShaders.find("Sprite");
    if (it != mShaders.end() && it->second)
    {
        it->second->SetActive();
        Matrix4 viewProj = Matrix4::CreateSimpleViewProj(mScreenWidth, mScreenHeight);
        it->second->SetMatrixUniform("uViewProj", viewProj);
        glUseProgram(0); // 保険（好み）
    }
}


//=============================================================
// UI / Virtual 解像度関連
//=============================================================

void Renderer::SetVirtualResolution(float w, float h)
{
    mVirtualWidth  = w;
    mVirtualHeight = h;
}

UIScaleInfo Renderer::GetUIScaleInfo() const
{
    // UIScaleInfo を計算して返す
    UIScaleInfo info{};

    // 物理解像度（必ずピクセルベース）
    info.screenW = mScreenWidth;
    info.screenH = mScreenHeight;

    // Virtual が未設定なら「物理＝論理」とみなす
    info.virtualW = (mVirtualWidth  > 0.0f) ? mVirtualWidth  : mScreenWidth;
    info.virtualH = (mVirtualHeight > 0.0f) ? mVirtualHeight : mScreenHeight;

    // 0除算回避
    if (info.virtualW <= 0.0f)
    {
        info.virtualW = 1.0f;
    }
    if (info.virtualH <= 0.0f)
    {
        info.virtualH = 1.0f;
    }

    info.scaleX = info.screenW / info.virtualW;
    info.scaleY = info.screenH / info.virtualH;

    // レターボックス前提の共通スケール
    info.scale = (info.scaleX < info.scaleY) ? info.scaleX : info.scaleY;

    return info;
}


//=============================================================
// シャドウマッピング
//=============================================================

bool Renderer::InitializeShadowMapping()
{
    // シャドウマップ用 FBO 初期化
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

void Renderer::RenderShadowMap()
{
    // 現在のFBO/Viewportを退避（RTTでも壊さない）
    GLint prevFBO = 0;
    GLint prevVP[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevVP);
    glEnable(GL_DEPTH_TEST);

    // ベースとなる中心とライト方向は共通
    Vector3 camCenter = mInvView.GetTranslation() + mInvView.GetZAxis() * 30.0f;
    Vector3 lightDir  = mLightingManager->GetLightDirection();
    Vector3 lightPos  = camCenter - lightDir * 50.0f;

    Matrix4 lightView = Matrix4::CreateLookAt(
        lightPos,
        camCenter,
        Vector3::UnitY
    );

    // Near / Far 用の ortho サイズ（まずは固定でOK）
    const float orthoW[kShadowCascadeCount] =
    {
        mShadowOrthoWidth,          // Near（既存値）
        mShadowOrthoWidth * 4.0f    // Far（とりあえず広げる。後で調整）
    };
    const float orthoH[kShadowCascadeCount] =
    {
        mShadowOrthoHeight,
        mShadowOrthoHeight * 4.0f
    };

    for (int i = 0; i < kShadowCascadeCount; ++i)
    {
        // --- FBO バインド & viewport ---
        glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO[i]);
        glViewport(0, 0, (GLsizei)mShadowFBOWidth, (GLsizei)mShadowFBOHeight);
        glClear(GL_DEPTH_BUFFER_BIT);

        // --- Light VP 構築（ToyLib流：view * proj） ---
        Matrix4 lightProj = Matrix4::CreateOrtho(
            orthoW[i],
            orthoH[i],
            mShadowNear,
            mShadowFar
        );

        Matrix4 lightVP = lightView * lightProj;
        mLightSpaceMatrix[i] = lightVP;

        // --- フラスタム作ってカリング ---
        Frustum shadowFrustum = BuildFrustumFromMatrix(lightVP);

        for (auto& visual : mVisualComps)
        {
            if (!visual->GetEnableShadow() || !visual->IsVisible())
                continue;

            // 影パスでも BoundingVolume があればカリング（既存のまま）
            Actor* owner = visual->GetOwner();
            if (owner)
            {
                auto bv = owner->GetComponent<BoundingVolumeComponent>();
                if (bv)
                {
                    Cube aabb = bv->GetWorldAABB();
                    if (!FrustumIntersectsAABB(shadowFrustum, aabb))
                        continue;
                }
            }

            visual->DrawShadow(i);
        }
    }

    // 戻す
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
}


//=============================================================
// その他ユーティリティ
//=============================================================

void Renderer::RegisterSkyDome(SkyDomeComponent* sky)
{
    mSkyDomeComp = sky;
    if (mSkyDomeComp)
    {
        // SkyDome にライティング情報を共有
        mSkyDomeComp->SetLightingManager(mLightingManager);
    }
}

void Renderer::SetClearColor(const Vector3& color)
{
    // クリアカラー変更
    mClearColor = color;
    glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);
}


//=============================================================
// シェーダーロード
//=============================================================

bool Renderer::LoadShaders()
{
    std::string vShaderName;
    std::string fShaderName;

    //---------------------------------------------------------
    // 天気オーバーレイ用シェーダー
    //---------------------------------------------------------
    vShaderName = mShaderPath + "WeatherScreen.vert";
    fShaderName = mShaderPath + "WeatherScreen.frag";
    mShaders["WeatherOverlay"] = std::make_shared<Shader>();
    if (!mShaders["WeatherOverlay"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // メッシュ用 Phong シェーダー
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Phong.vert";
    fShaderName = mShaderPath + "Phong.frag";
    mShaders["Mesh"] = std::make_shared<Shader>();
    if (!mShaders["Mesh"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // スキンメッシュ用（頂点のみ差し替え）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Skinned.vert";
    fShaderName = mShaderPath + "Phong.frag";
    mShaders["Skinned"] = std::make_shared<Shader>();
    if (!mShaders["Skinned"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // スプライト用
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Sprite.vert";
    fShaderName = mShaderPath + "Sprite.frag";
    mShaders["Sprite"] = std::make_shared<Shader>();
    if (!mShaders["Sprite"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    // Sprite は 2D 用 ViewProj を最初に渡しておく
    mShaders["Sprite"]->SetActive();
    Matrix4 viewProj = Matrix4::CreateSimpleViewProj(mScreenWidth, mScreenHeight);
    mShaders["Sprite"]->SetMatrixUniform("uViewProj", viewProj);

    //---------------------------------------------------------
    // ビルボード
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Billboard.vert";
    fShaderName = mShaderPath + "Billboard.frag";
    mShaders["Billboard"] = std::make_shared<Shader>();
    if (!mShaders["Billboard"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // パーティクル
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Billboard.vert";
    fShaderName = mShaderPath + "Particle.frag";
    mShaders["Particle"] = std::make_shared<Shader>();
    if (!mShaders["Particle"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // GPUパーティクル（Update用：Transform Feedback）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "ParticleUpdate.vert";
    auto update = std::make_shared<Shader>();
    update->LoadWithTransformFeedback(
        vShaderName,
        "", // fragなし
        { "tfPos", "tfVel", "tfLife" },
        GL_INTERLEAVED_ATTRIBS
    );
    mShaders["ParticleUpdate"] = update;

    //---------------------------------------------------------
    // GPUパーティクル（Render用）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "ParticleGPU.vert";
    fShaderName = mShaderPath + "ParticleGPU.frag";
    auto render = std::make_shared<Shader>();
    render->Load(vShaderName, fShaderName);
    mShaders["ParticleGPU"] = render;

    //---------------------------------------------------------
    // ソリッドカラー（ワイヤーフレーム／デバッグ用など）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "BasicMesh.vert";
    fShaderName = mShaderPath + "SolidColor.frag";
    mShaders["Solid"] = std::make_shared<Shader>();
    if (!mShaders["Solid"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // シャドウマップ（スキンメッシュ）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "ShadowMapping_Skinned.vert";
    fShaderName = mShaderPath + "ShadowMapping.frag";
    mShaders["ShadowSkinned"] = std::make_shared<Shader>();
    if (!mShaders["ShadowSkinned"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // シャドウマップ（通常メッシュ）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "ShadowMapping_Mesh.vert";
    fShaderName = mShaderPath + "ShadowMapping.frag";
    mShaders["ShadowMesh"] = std::make_shared<Shader>();
    if (!mShaders["ShadowMesh"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // RenderSurface（鏡/モニタ用の映像面）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "RenderSurface.vert";
    fShaderName = mShaderPath + "RenderSurface.frag";
    mShaders["RenderSurface"] = std::make_shared<Shader>();
    if (!mShaders["RenderSurface"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // スカイドーム（時間帯・天候ベースの空）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "WeatherDome.vert";
    fShaderName = mShaderPath + "WeatherDome.frag";
    mShaders["SkyDome"] = std::make_shared<Shader>();
    if (!mShaders["SkyDome"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }

    //---------------------------------------------------------
    // デフォルトのビュー／プロジェクション行列
    //---------------------------------------------------------
    mViewMatrix = Matrix4::CreateLookAt(
        Vector3(0, 0.5f, -3),
        Vector3(0, 0, 10),
        Vector3::UnitY
    );
    mProjectionMatrix = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mPerspectiveFOV),
        mScreenWidth,
        mScreenHeight,
        1.0f,
        2000.0f
    );

    return true;
}

std::shared_ptr<toy::Shader> toy::Renderer::GetShader(const std::string& name)
{
    // 名前から Shader を取得（なければ nullptr）
    auto itr = mShaders.find(name);
    return (itr != mShaders.end()) ? itr->second : nullptr;
}


//=============================================================
// テキスト → テクスチャ生成（SDL3_ttf）
//  - 改行(\n)対応版
//=============================================================

std::shared_ptr<Texture> Renderer::CreateTextTexture(
    const std::string& text,
    const Vector3& color,
    std::shared_ptr<TextFont> font)
{
    if (!font || !font->IsValid())
    {
        std::cerr << "[Renderer] CreateTextTexture: invalid font" << std::endl;
        return nullptr;
    }

    if (text.empty())
    {
        return nullptr;
    }

    TTF_Font* nativeFont = font->GetNativeFont();

    SDL_Color sdlColor;
    sdlColor.r = static_cast<Uint8>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
    sdlColor.g = static_cast<Uint8>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
    sdlColor.b = static_cast<Uint8>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
    sdlColor.a = 255;

    // --------------------------------------------------------
    // まず、改行がないなら従来どおり 1 行で描画して終了
    // --------------------------------------------------------
    if (text.find('\n') == std::string::npos)
    {
        SDL_Surface* surface = TTF_RenderText_Blended(
            nativeFont,
            text.c_str(),
            static_cast<int>(text.size()),
            sdlColor
        );

        if (!surface)
        {
            std::cerr << "[Renderer] TTF_RenderText_Blended failed: "
                      << SDL_GetError() << std::endl;
            return nullptr;
        }

        SDL_Surface* conv = SDL_ConvertSurface(
            surface,
            SDL_PIXELFORMAT_RGBA32
        );
        SDL_DestroySurface(surface);

        if (!conv)
        {
            std::cerr << "[Renderer] SDL_ConvertSurface failed: "
                      << SDL_GetError() << std::endl;
            return nullptr;
        }

        const int   width  = conv->w;
        const int   height = conv->h;
        const void* pixels = conv->pixels;

        auto tex = std::make_shared<Texture>();
        if (!tex->CreateFromPixels(pixels, width, height, /*hasAlpha=*/true))
        {
            SDL_DestroySurface(conv);
            std::cerr << "[Renderer] CreateFromPixels failed" << std::endl;
            return nullptr;
        }

        SDL_DestroySurface(conv);
        return tex;
    }

    // --------------------------------------------------------
    // ここから：複数行テキスト対応
    // --------------------------------------------------------

    // 簡単な改行分割（StringUtil にあればそちらを使ってもOK）
    std::vector<std::string> lines;
    {
        std::string current;
        for (char c : text)
        {
            if (c == '\n')
            {
                lines.push_back(current);
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        lines.push_back(current);
    }

    std::vector<SDL_Surface*> lineSurfaces;
    lineSurfaces.reserve(lines.size());

    int maxW   = 0;
    int totalH = 0;

    for (auto& line : lines)
    {
        // 空行の場合も高さだけは欲しいので、スペース 1 文字で代用
        const std::string& drawStr = line.empty() ? std::string(" ") : line;

        SDL_Surface* s = TTF_RenderText_Blended(
            nativeFont,
            drawStr.c_str(),
            static_cast<int>(drawStr.size()),
            sdlColor
        );

        if (!s)
        {
            std::cerr << "[Renderer] TTF_RenderText_Blended (multi-line) failed: "
                      << SDL_GetError() << std::endl;
            continue;
        }

        lineSurfaces.push_back(s);
        maxW   = std::max(maxW, s->w);
        totalH += s->h;
    }

    if (lineSurfaces.empty())
    {
        std::cerr << "[Renderer] CreateTextTexture: no valid line surfaces" << std::endl;
        return nullptr;
    }

    // 1枚目のフォーマットを基準に合成用のサーフェスを作成
    SDL_Surface* combined = SDL_CreateSurface(
        maxW,
        totalH,
        SDL_PIXELFORMAT_RGBA32
    );

    if (!combined)
    {
        std::cerr << "[Renderer] SDL_CreateSurface (combined) failed: "
                  << SDL_GetError() << std::endl;
        for (auto* s : lineSurfaces) SDL_DestroySurface(s);
        return nullptr;
    }

    // 縦方向に順番に貼っていく
    int offsetY = 0;
    for (auto* s : lineSurfaces)
    {
        SDL_Rect dst;
        dst.x = 0;
        dst.y = offsetY;
        dst.w = s->w;
        dst.h = s->h;

        SDL_BlitSurface(s, nullptr, combined, &dst);
        offsetY += s->h;

        SDL_DestroySurface(s);
    }
    lineSurfaces.clear();

    // RGBA32 に変換（従来と同じ流れ）
    SDL_Surface* conv = SDL_ConvertSurface(
        combined,
        SDL_PIXELFORMAT_RGBA32
    );
    SDL_DestroySurface(combined);

    if (!conv)
    {
        std::cerr << "[Renderer] SDL_ConvertSurface (combined) failed: "
                  << SDL_GetError() << std::endl;
        return nullptr;
    }

    const int   width  = conv->w;
    const int   height = conv->h;
    const void* pixels = conv->pixels;

    auto tex = std::make_shared<Texture>();
    if (!tex->CreateFromPixels(pixels, width, height, /*hasAlpha=*/true))
    {
        SDL_DestroySurface(conv);
        std::cerr << "[Renderer] CreateFromPixels (combined) failed" << std::endl;
        return nullptr;
    }

    SDL_DestroySurface(conv);
    return tex;
}


//=============================================================
// 2Dカメラの視界に入っているか（World → Screen）
//=============================================================

ScreenProjectResult Renderer::WorldToScreen(const Vector3& worldPos) const
{
    ScreenProjectResult result{};
    result.visible = false;
    result.screen  = Vector2::Zero;
    result.depth   = 1.0f;

    Matrix4 view = GetViewMatrix();
    Matrix4 proj = GetProjectionMatrix();
    Matrix4 viewProj = view * proj;   // ToyLib 流

    Vector3 ndc = Vector3::TransformWithPerspDiv(worldPos, viewProj, 1.0f);

    float ndcX = ndc.x;
    float ndcY = ndc.y;
    float ndcZ = ndc.z;

    if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ))
    {
        return result;
    }

    // ★ここだけでクリップ判定を完結させる
    if (ndcX < -1.0f || ndcX >  1.0f ||
        ndcY < -1.0f || ndcY >  1.0f ||
        ndcZ <  0.0f || ndcZ >  1.0f)
    {
        return result;
    }

    float screenX = (ndcX * 0.5f + 0.5f) * GetScreenWidth();
    float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * GetScreenHeight();

    result.visible = true;
    result.screen  = Vector2(screenX, screenY);
    result.depth   = ndcZ;
    return result;
}

} // namespace toy
