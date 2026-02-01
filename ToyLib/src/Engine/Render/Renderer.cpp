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
#include "Graphics/Effect/RenderSurfaceComponent.h"
#include "Graphics/Mesh/MeshComponent.h"
#include "Graphics/Mesh/SkeletalMeshComponent.h"
#include "Graphics/Billboard/BillboardComponent.h"
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

bool Renderer::Initialize(SDL_Window* window, SDL_GLContext glContext)
{


    mWindow = window;     // 非所有
    mGLContext = glContext;  // 非所有（destroyしない）





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

    //---------------------------------------------------------
    // ビューポート＆射影行列など、サイズ依存の状態をまとめて更新
    //---------------------------------------------------------
    OnWindowResized(pixelW, pixelH);

    std::cerr << "[Renderer] GL Init Complete. "
              << "Pixels("  << pixelW   << "x" << pixelH   << ") "
              << "Scale="   << mWindowDisplayScale
              << std::endl;

    return true;
}

void Renderer::Shutdown()
{
    // 1) まず current を保証（できなければ何もしない）
    if (!mWindow || !mGLContext)
        return;

    if (SDL_GL_MakeCurrent(mWindow, mGLContext) != 0)
        return;

    // 2) ここから先は GL を触ってOK

    // Shadow textures（Unload が glDeleteTextures する前提）
    for (auto& tex : mShadowMapTexture)
    {
        if (tex)
        {
            tex->Unload();
            tex.reset();
        }
    }

    // FBO（mShadowFBO が必ず 0 初期化されている前提）
    glDeleteFramebuffers(kShadowCascadeCount, mShadowFBO);
    for (int i = 0; i < kShadowCascadeCount; ++i)
    {
        mShadowFBO[i] = 0;
        mLightSpaceMatrix[i] = Matrix4::Identity;
    }

    // VAO/VBO 等（デストラクタで glDelete* するならここで current 必須）
    mFullScreenQuad.reset();
    mSpriteVerts.reset();
    mSurfaceQuad.reset();
}

//=============================================================
// メイン描画パス(新パス)
//=============================================================
void Renderer::BeginFrame()
{
    const bool usePost = (mPost.type != PostEffectType::None);

    if (usePost)
    {
        if (!mSceneRT ||
            mSceneRT->GetWidth()  != (int)mScreenWidth ||
            mSceneRT->GetHeight() != (int)mScreenHeight)
        {
            mSceneRT = std::make_shared<RenderTarget>();
            mSceneRT->Create((int)mScreenWidth, (int)mScreenHeight);
        }

        // ★ここ
        mSceneRT->Bind();
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);
    }

    // 以下いつもの GL state 初期化
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::RenderShadowPass()
{
    // 現在のFBO/Viewportを退避（RTTでも壊さない）
    GLint prevFBO = 0;
    GLint prevVP[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevVP);

    // Shadow pass の期待状態
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

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
        mShadowOrthoWidth,
        mShadowOrthoWidth * 4.0f
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

        // --- Shadow 専用 RenderQueue を構築 ---
        RenderQueue queue;

        for (auto* vc : mVisualComps)
        {
            if (!vc || !vc->GetEnableShadow() || !vc->IsVisible())
                continue;

            // 影パスでも BoundingVolume があればカリング
            Actor* owner = vc->GetOwner();
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

            // ★新：Shadow用アイテムを積む
            vc->GatherShadowItems(queue);//, i, lightVP);
        }

        // ★影パス描画（RenderItemを回す）
        DrawRenderQueue_Shadow(queue, i);
    }

    // 戻す（color mask 含めて確実に）
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
}
void Renderer::RestoreAfterShadowPass()
{
    glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glDisable(GL_STENCIL_TEST);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::DrawSkyPass()
{
    if (mQ_Sky.Items().empty())
    {
        return;
    }

    // Sky 基本 state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    mQ_Sky.Sort();
    DrawRenderQueue_World(mQ_Sky);

    // 戻す（混在期の保険）
    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::DrawWorldPass()
{
    //=========================================================
    // 2) WORLD OPAQUE（3D）：Object3D の Opaque だけ
    //=========================================================
    {
        RenderQueue worldOpaqueQueue;

        for (const auto& it : mQ_Object3D.Items())
        {
            if (it.blend == BlendMode::Opaque)
            {
                worldOpaqueQueue.Push(it);
            }
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glDisable(GL_BLEND);

        worldOpaqueQueue.Sort();
        DrawRenderQueue_World(worldOpaqueQueue);

        // 混在期の保険：戻す
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //=========================================================
    // 3) WORLD EFFECT (PRE)（3D）：深度に従う “貼り付き/足元系”
    //=========================================================
    {
        RenderQueue effectPreQueue;

        for (const auto& it : mQ_Effect3D.Items())
        {
            const bool isPre =
                (it.type == RenderItemType::Sprite) ||
                (it.type == RenderItemType::Debug);

            if (isPre)
            {
                effectPreQueue.Push(it);
            }
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        effectPreQueue.Sort();
        DrawRenderQueue_World(effectPreQueue);

        // 保険：戻す
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    //=========================================================
    // 4) WORLD TRANSPARENT（3D）：Object3D の透明（Alpha/Additive）
    //=========================================================
    {
        RenderQueue transparentQueue;

        for (const auto& it : mQ_Object3D.Items())
        {
            if (it.blend != BlendMode::Opaque)
            {
                transparentQueue.Push(it);
            }
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        transparentQueue.Sort();
        DrawRenderQueue_World(transparentQueue);

        // 混在期の保険：戻す
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    //=========================================================
    // 5) WORLD EFFECT (OVERLAY)（3D）：発光/粒/上に重ねたいもの
    //=========================================================
    {
        RenderQueue effectOverlayQueue;

        for (const auto& it : mQ_Effect3D.Items())
        {
            const bool isOverlay =
                (it.type == RenderItemType::Billboard) ||
                (it.type == RenderItemType::GPUParticle);

            if (isOverlay)
            {
                effectOverlayQueue.Push(it);
            }
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        effectOverlayQueue.Sort();
        DrawRenderQueue_World(effectOverlayQueue);

        // 混在期の保険：World標準へ戻す
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }
}
void Renderer::DrawOverlayScreenPass()
{
    if (mQ_OverlayScreen.Items().empty())
    {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    // いまは固定（将来は RenderItem.blend で切り替える）
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mQ_OverlayScreen.Sort();
    DrawRenderQueue_World(mQ_OverlayScreen);

    // 戻す（混在期の保険）
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}
void Renderer::DrawFadePass()
{
    if (!mEnableFade)
    {
        return;
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto sh = GetShader("Fade");
    if (!sh)
    {
        return;
    }
    
    sh->SetActive();
    sh->SetVectorUniform("uColor", mFadeColor);
    sh->SetFloatUniform("uAlpha", mFadeAlpha);

    mFullScreenQuad->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    glDisable(GL_BLEND);
}
void Renderer::DrawUIPass()
{
    if (mQ_UI.Items().empty())
    {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mQ_UI.Sort();
    DrawRenderQueue_World(mQ_UI);

    // 戻す（混在期の保険）
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}
void Renderer::DrawPostEffectPass()
{
    if (!mSceneRT)
    {
        return;
    }
    
    auto sceneTex = mSceneRT->GetColorTexture();
    if (!sceneTex)
    {
        return;
    }
    // ここは「上書き」したいのでブレンド切るのが無難
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    auto sh = GetShader("PostEffect");
    if (!sh)
    {
        return;
    }
    sh->SetActive();

    // sampler2D
    // Texture::SetActive(unit) が glActiveTexture + glBindTexture をやってくれる
    sceneTex->SetActive(0);
    sh->SetTextureUniform("uSceneTex", 0);

    switch (mPost.type)
    {
        case PostEffectType::None:
        case PostEffectType::Sepia:
        case PostEffectType::CRT:
            // Post settings
            sh->SetIntUniform("uPostType", static_cast<int>(mPost.type));
            sh->SetFloatUniform("uIntensity", mPost.intensity);
            
            // 任意：時間（CRTノイズ等）
            sh->SetFloatUniform("uTime",  SDL_GetTicks());   // 累積秒
            sh->SetIntUniform("uFlipY", 0);           // 上下逆なら 1 に
            break;
        case PostEffectType::FeilyLand:
            sh->SetIntUniform("uPostType", 3);        // FairyLand
            sh->SetFloatUniform("uIntensity", 1.0f);
            sh->SetFloatUniform("uTime", SDL_GetTicks());
            sh->SetIntUniform("uFlipY", 0);
            
            sh->SetIntUniform("uUsePaperTex", 0);
            break;
        case PostEffectType::Watercolor:
            sh->SetIntUniform("uPostType", 4);
            sh->SetFloatUniform("uIntensity", 1.0f);
            sh->SetFloatUniform("uTime", SDL_GetTicks());
            
            sh->SetIntUniform("uUsePaperTex", 1);
            if (mPost.paperTex)
            {
                mPost.paperTex->SetActive(1);
                sh->SetTextureUniform("uPaperTex", 1);
            }
            else
            {
                sh->SetTextureUniform("uPaperTex", 0);
            }
            break;
    }

    // Draw fullscreen
    mFullScreenQuad->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    // 復帰
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}


void Renderer::EndFrame()
{
    SDL_GL_SwapWindow(mWindow);
}
//=============================================================
// メイン描画パス
//=============================================================
void Renderer::Draw()
{
    ResetDebugCounter();
    
    //=========================
    // 0) SceneCapture (RTT)
    //=========================
    for (const auto& req : mSceneCaptureQueue)
    {
        DrawToRenderTarget(req.rt, req.view, req.proj, req.drawUI);
    }
    mSceneCaptureQueue.clear();
    
    //=========================================================
    // 0) Frame begin : 強制的に GL state を揃える（混在期の事故防止）
    //=========================================================
    BeginFrame();
    BuildFrameQueues();
    //=========================================================
    // 1) SHADOW : ShadowMap を作る（CSM）
    //=========================================================
    RenderShadowPass();
    RestoreAfterShadowPass();

    //=========================================================
    // 2) SKY（背景）
    //   - SkyDome を最初に描く
    //=========================================================
    DrawSkyPass();
    //=========================================================
    // 3) World
    //=========================================================
    DrawWorldPass();
    
    //=========================================================
    // 4) OVERLAY SCREEN：画面全体の後処理系（深度OFF）
    //=========================================================
    DrawOverlayScreenPass();
    //=========================================================
    // 5) OVERLAY SCREEN：画面全体の後処理系（深度OFF）
    //=========================================================
    if (mPost.type != PostEffectType::None)
    {
        RenderTarget::Unbind();
        glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);
        DrawPostEffectPass();
    }
    //=========================================================
    // 5) UI：Sprite（深度OFF）
    //=========================================================
    DrawUIPass();

    //=========================================================
    // 6) Fade
    //=========================================================
    DrawFadePass();
    //=========================================================
    // 7) Present
    //=========================================================
    EndFrame();
}

// レンダーキュー構築
void Renderer::BuildFrameQueues()
{
    mQ_Sky.Clear();
    mQ_Object3D.Clear();
    mQ_Effect3D.Clear();
    mQ_OverlayScreen.Clear();
    mQ_UI.Clear();

    //========================================
    // 1) Frustum を1回だけ作る
    //========================================
    const Matrix4 view = GetViewMatrix();
    const Matrix4 proj = GetProjectionMatrix();
    const Matrix4 vp   = view * proj;

    const Frustum fr = BuildFrustumFromMatrix(vp);

    RenderQueue tmp;

    for (auto* vc : mVisualComps)
    {
        if (!vc) continue;
        if (!vc->IsVisible()) continue;

        const VisualLayer layer = vc->GetLayer();

        //========================================
        // 2) レイヤーでカリング対象を絞る
        //========================================
        const bool shouldCull =
            (layer == VisualLayer::Object3D) ||
            (layer == VisualLayer::Effect3D);

        if (shouldCull)
        {
            Actor* owner = vc->GetOwner();
            if (owner)
            {
                auto* bv = owner->GetComponent<BoundingVolumeComponent>();
                if (bv)
                {
                    const Cube aabb = bv->GetWorldAABB();
                    if (!FrustumIntersectsAABB(fr, aabb))
                    {
                        continue; // ★ここで Gather 自体をスキップ
                    }
                }
            }
        }

        //========================================
        // 3) Gather → layer / pass で分配
        //========================================
        tmp.Clear();
        vc->GatherRenderItems(tmp);

        for (const auto& it : tmp.Items())
        {
            if (it.pass == RenderPass::UI || it.layer == VisualLayer::UI)
            {
                mQ_UI.Push(it);
                continue;
            }

            switch (it.layer)
            {
                case VisualLayer::Sky:           mQ_Sky.Push(it); break;
                case VisualLayer::Object3D:      mQ_Object3D.Push(it); break;
                case VisualLayer::Effect3D:      mQ_Effect3D.Push(it); break;
                case VisualLayer::OverlayScreen: mQ_OverlayScreen.Push(it); break;
                case VisualLayer::UI:            mQ_UI.Push(it); break;
                default:                         mQ_Object3D.Push(it); break;
            }
        }
    }
}


//=============================================================
// カメラ操作
//=============================================================

void Renderer::PushCameraState()
{
    CameraState s{};
    s.view    = mViewMatrix;
    s.proj    = mProjectionMatrix;
    s.viewProj = mViewMatrix * mProjectionMatrix;
    s.invView = mInvView;

    mCameraStack.push_back(s);
}

void Renderer::SetCameraState(const CameraState& s)
{
    mViewMatrix       = s.view;
    mProjectionMatrix = s.proj;
    mInvView          = s.invView;

    // viewProj は保持してるけど、Renderer側は “都度計算” 方式でもOK。
    // もし Renderer に mViewProj をメンバで持つなら、ここで同期。
    // mViewProj = s.viewProj;
}

void Renderer::PopCameraState()
{
    if (mCameraStack.empty())
        return;

    const CameraState s = mCameraStack.back();
    mCameraStack.pop_back();
    SetCameraState(s);
}


//=============================================================
// シーンキャプチャーリクエスト
//=============================================================

void Renderer::RequestSceneCapture(const SceneCaptureRequest& req)
{
    // 無効チェック（最低限）
    if (!req.rt)
    {
        return;
    }
    mSceneCaptureQueue.push_back(req);
}

void Renderer::DrawToRenderTarget(const std::shared_ptr<RenderTarget>& rt,
                                  const Matrix4& view,
                                  const Matrix4& proj,
                                  bool drawUI)
{
    if (!rt) return;
    
    ChangeDebugRTT();

    const Matrix4 prevView = mViewMatrix;
    const Matrix4 prevProj = mProjectionMatrix;

    GLint prevVP[4];
    glGetIntegerv(GL_VIEWPORT, prevVP);

    rt->Bind();                       // ★ここで RT viewport
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mViewMatrix       = view;
    mProjectionMatrix = proj;

    // --- 影パス（必要な場合） ---
    RenderShadowPass();

    // ★これが重要：RestoreAfterShadowPass が画面viewportへ戻すので
    // RT viewportに戻し直す（旧コードに近い動きになる）
    RestoreAfterShadowPass();
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // ※BindはRestore側で壊してないなら不要
    rt->Bind(); // ★viewport を RT に戻す（FBOも戻るのでこの形が確実）
    // ↑「FBOはrtのままでviewportだけ戻したい」なら glViewport(0,0,rt->GetWidth(),rt->GetHeight()) でもOK

    BuildFrameQueues();

    DrawSkyPass();
    DrawWorldPass();
    DrawOverlayScreenPass();
    if (drawUI) DrawUIPass();
    DrawFadePass();

    // 復帰
    mViewMatrix       = prevView;
    mProjectionMatrix = prevProj;

    RenderTarget::Unbind();
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
    
    ChangeDebugOnScreen();

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
    mSceneRT->Unload();
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

    // ---- ここで SceneRT を用意（ポスト用） ----
    if (!mSceneRT)
    {
        mSceneRT = std::make_shared<RenderTarget>();
        if (!mSceneRT->Create(pixelW, pixelH))
        {
            std::cerr << "[Renderer] Failed to create SceneRT\n";
            mSceneRT.reset();
        }
    }
    else
    {
        // 今は作り直し（※ RenderTarget側が解放できる設計にしてからが理想）
        mSceneRT = std::make_shared<RenderTarget>();
        if (!mSceneRT->Create(pixelW, pixelH))
        {
            std::cerr << "[Renderer] Failed to recreate SceneRT\n";
            mSceneRT.reset();
        }
    }
    
    // DPI スケールもここで取り直しておくと、モニタ跨ぎ時も安全
    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f)
    {
        mWindowDisplayScale = 1.0f;
    }

    // FOV生成
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


//=============================================================
// その他ユーティリティ
//=============================================================
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
    // ポストエフェクト（フルスクリーン）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "PostEffect.vert";
    fShaderName = mShaderPath + "PostEffect.frag";
    mShaders["PostEffect"] = std::make_shared<Shader>();
    if (!mShaders["PostEffect"]->Load(vShaderName.c_str(), fShaderName.c_str()))
    {
        return false;
    }
    
    //---------------------------------------------------------
    // フェード（フルスクリーン）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "PostEffect.vert";
    fShaderName = mShaderPath + "Fade.frag";
    mShaders["Fade"] = std::make_shared<Shader>();
    if (!mShaders["Fade"]->Load(vShaderName.c_str(), fShaderName.c_str()))
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
    // Unlit （ビルボード:ライティングなし）
    //---------------------------------------------------------
    vShaderName = mShaderPath + "Unlit.vert";
    fShaderName = mShaderPath + "Unlit.frag";
    mShaders["Unlit"] = std::make_shared<Shader>();
    if (!mShaders["Unlit"]->Load(vShaderName.c_str(), fShaderName.c_str()))
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
    
    float dpiScale = GetUIScaleInfo().scale;
    
    float virtualX = screenX / dpiScale;
    float virtualY = screenY / dpiScale;

    result.visible = true;
    result.screen  = Vector2(screenX, screenY);
    result.virtualScreen = Vector2(virtualX, virtualY);
    result.depth   = ndcZ;
    return result;
}

} // namespace toy
