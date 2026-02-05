//==============================================================================
// Renderer_DrawPass.cpp
//  - RenderQueue draw helpers
//  - GL state apply
//  - Shadow/World/UI/Post passes
//  - Draw(), BuildFrameQueues(), DrawToRenderTarget()
//==============================================================================

#include "Engine/Render/Renderer.h"

// Engine / Render
#include "Engine/Render/LightingManager.h"
#include "Engine/Render/RenderTarget.h"
#include "Engine/Render/Shader.h"

// Geometry
#include "Asset/Geometry/VertexArray.h"

// Graphics
#include "Graphics/VisualComponent.h"

// Physics / Core
#include "Physics/BoundingVolumeComponent.h"
#include "Engine/Core/Actor.h"

// Utils
#include "Utils/FrustumUtil.h"

// GL
#include "glad/glad.h"

// Std
#include <iostream>

namespace toy {

//----------------------------------------
// 共通：RenderQueue draw
//----------------------------------------
void Renderer::DrawBucket_World(const std::vector<uint32_t>& bucket)
{
    const auto& items = mFrame.items;

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size()) continue;

        const RenderItem& it = items[idx];
        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem_GL(it, RenderPass::World, -1);
    }
}


void Renderer::DrawBucket_Shadow(const std::vector<uint32_t>& bucket, int cascadeIndex)
{
    if (bucket.empty())
    {
        return;
    }

    const auto& items = mFrame.items;

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size())
        {
            continue; // 安全弁
        }

        const RenderItem& it = items[idx];

        // Shadowで描かないもの（UIなど）を除外したいならここで弾く
        // ※ bucket側で分けてるなら不要
        if (it.pass == RenderPass::UI || it.layer == VisualLayer::UI)
        {
            continue;
        }

        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem_GL(it, RenderPass::Shadow, cascadeIndex);
    }
}
//----------------------------------------
// State apply
//----------------------------------------
void Renderer::ApplyState_GL(const RenderItem& it)
{
    // depth
    if (it.depthTest) glEnable(GL_DEPTH_TEST);
    else             glDisable(GL_DEPTH_TEST);

    if (it.type == RenderItemType::SkyDome) glDepthFunc(GL_LEQUAL);
    else                                   glDepthFunc(GL_LESS);

    glDepthMask(it.depthWrite ? GL_TRUE : GL_FALSE);

    // blend
    if (it.blend == BlendMode::Opaque)
    {
        glDisable(GL_BLEND);
    }
    else
    {
        glEnable(GL_BLEND);
        if (it.blend == BlendMode::Additive) glBlendFunc(GL_ONE, GL_ONE);
        else                                 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // cull
    if (it.cull == CullMode::None)
    {
        glDisable(GL_CULL_FACE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(it.cull == CullMode::Back ? GL_BACK : GL_FRONT);
    }

    // front face
    glFrontFace(it.frontFace == FrontFace::CCW ? GL_CCW : GL_CW);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

//----------------------------------------
// helpers（このcpp内限定）
//----------------------------------------
namespace {

inline bool ValidateGeometryForDraw(const RenderItem& it)
{
    const bool isParticle = (it.type == RenderItemType::Particle);

    if (!isParticle)
    {
        if (!it.geometry.ptr) return false;
        const bool hasElements = (it.indexCount  > 0);
        const bool hasArrays   = (it.vertexCount > 0);
        if (!hasElements && !hasArrays) return false;
    }
    else
    {
        if (it.gpuVAO == 0 || it.instanceCount <= 0 || it.indexCount <= 0)
            return false;
    }
    return true;
}

inline void SetCommonUniforms(Renderer& r,
                             const RenderItem& it,
                             RenderPass pass,
                             int cascadeIndex)
{
    auto* sh = it.shader.ptr;
    if (!sh) return;

    if (pass == RenderPass::World || pass == RenderPass::UI)
    {
        sh->SetMatrixUniform("uViewProj",       it.viewProj);
        sh->SetMatrixUniform("uWorldTransform", it.world);
    }
    else if (pass == RenderPass::Shadow)
    {
        sh->SetMatrixUniform("uWorldTransform",   it.world);
        sh->SetMatrixUniform("uLightSpaceMatrix", r.GetLightSpaceMatrix(cascadeIndex));
    }
}

inline void DrawDefaultGeometry_GL(Renderer& r, const RenderItem& it)
{
    if (it.type == RenderItemType::Particle)
    {
        glBindVertexArray(it.gpuVAO);
        glDrawElementsInstanced(GL_TRIANGLES,
                                it.indexCount,
                                GL_UNSIGNED_INT,
                                nullptr,
                                it.instanceCount);
        glBindVertexArray(0);
        r.AddDrawCall();
        return;
    }

    it.geometry.ptr->SetActive();

    GLenum mode = GL_TRIANGLES;
    switch (it.topology)
    {
        case PrimitiveTopology::Triangles: mode = GL_TRIANGLES; break;
        case PrimitiveTopology::Lines:     mode = GL_LINES;     break;
        default:                           mode = GL_TRIANGLES; break;
    }

    if (it.indexCount > 0)
        glDrawElements(mode, it.indexCount, GL_UNSIGNED_INT, nullptr);
    else
        glDrawArrays(mode, 0, it.vertexCount);

    r.AddDrawCall();
}

} // unnamed namespace

//----------------------------------------
// DrawItem_GL
//----------------------------------------
void Renderer::DrawItem_GL(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    if (!ValidateGeometryForDraw(it)) return;

    ApplyState_GL(it);

    if (!it.shader.ptr) return;
    it.shader.ptr->SetActive();

    SetCommonUniforms(*this, it, pass, cascadeIndex);

    if (it.dispatch)
    {
        const bool alreadyDrawn = it.dispatch(*this, it, pass, cascadeIndex);
        if (alreadyDrawn) return;
    }

    DrawDefaultGeometry_GL(*this, it);
}

//=============================================================
// Frame begin/end
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
        mSceneRT->Bind();
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::EndFrame()
{
    SDL_GL_SwapWindow(mWindow);
}

//=============================================================
// Shadow pass
//=============================================================
void Renderer::RenderShadowPass()
{
    GLint prevFBO = 0;
    GLint prevVP[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevVP);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    Vector3 camCenter = mInvView.GetTranslation() + mInvView.GetZAxis() * 30.0f;
    Vector3 lightDir  = mLightingManager->GetLightDirection();
    Vector3 lightPos  = camCenter - lightDir * 50.0f;

    Matrix4 lightView = Matrix4::CreateLookAt(lightPos, camCenter, Vector3::UnitY);

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
        glBindFramebuffer(GL_FRAMEBUFFER, mShadowFBO[i]);
        glViewport(0, 0, (GLsizei)mShadowFBOWidth, (GLsizei)mShadowFBOHeight);
        glClear(GL_DEPTH_BUFFER_BIT);

        Matrix4 lightProj = Matrix4::CreateOrtho(
            orthoW[i],
            orthoH[i],
            mShadowNear,
            mShadowFar
        );

        Matrix4 lightVP = lightView * lightProj;
        mLightSpaceMatrix[i] = lightVP;

        // ★ここで bucket を描くだけ
        DrawBucket_Shadow(mBuckets.shadowCaster, i);
    }

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

//=============================================================
// Individual passes
//=============================================================
void Renderer::DrawSkyPass()
{
    if (mBuckets.sky.empty()) return;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // 事前に BuildFrameQueues() で分類＆ソート済み
    DrawBucket_World(mBuckets.sky);

    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::DrawWorldPass()
{
    // 2) WORLD OPAQUE
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glDisable(GL_BLEND);

        // 事前に BuildFrameQueues() で分類＆ソート済み
        DrawBucket_World(mBuckets.worldOpaque);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // 3) WORLD EFFECT (PRE)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        DrawBucket_World(mBuckets.effectPre);

        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    // 4) WORLD TRANSPARENT
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        DrawBucket_World(mBuckets.worldTransparent);

        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    // 5) WORLD EFFECT (OVERLAY)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        DrawBucket_World(mBuckets.effectOverlay);

        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }
}

void Renderer::DrawOverlayScreenPass()
{
    if (mBuckets.overlayScreen.empty()) return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 事前に BuildFrameQueues() で分類＆ソート済み
    DrawBucket_World(mBuckets.overlayScreen);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void Renderer::DrawFadePass()
{
    if (!mEnableFade) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto sh = GetShader("Fade");
    if (!sh) return;

    sh->SetActive();
    sh->SetVectorUniform("uColor", mFadeColor);
    sh->SetFloatUniform("uAlpha", mFadeAlpha);

    mFullScreenQuad->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    glDisable(GL_BLEND);
}

void Renderer::DrawUIPass()
{
    if (mBuckets.ui.empty()) return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 事前に BuildFrameQueues() で分類＆ソート済み
    DrawBucket_World(mBuckets.ui);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void Renderer::DrawPostEffectPass()
{
    if (!mSceneRT) return;

    auto sceneTex = mSceneRT->GetColorTexture();
    if (!sceneTex) return;

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    auto sh = GetShader("PostEffect");
    if (!sh) return;

    sh->SetActive();

    sceneTex->SetActive(0);
    sh->SetTextureUniform("uSceneTex", 0);

    switch (mPost.type)
    {
        case PostEffectType::None:
        case PostEffectType::Sepia:
        case PostEffectType::CRT:
            sh->SetIntUniform("uPostType", static_cast<int>(mPost.type));
            sh->SetFloatUniform("uIntensity", mPost.intensity);
            sh->SetFloatUniform("uTime",  SDL_GetTicks());
            sh->SetIntUniform("uFlipY", 0);
            break;

        case PostEffectType::FeilyLand:
            sh->SetIntUniform("uPostType", 3);
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

    mFullScreenQuad->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

//=============================================================
// Main Draw
//=============================================================
void Renderer::Draw()
{
    ResetDebugCounter();

    // SceneCapture RTT
    for (const auto& req : mSceneCaptureQueue)
    {
        DrawToRenderTarget(req);
    }
    mSceneCaptureQueue.clear();

    BeginFrame();
    BuildFrameQueues();

    RenderShadowPass();
    RestoreAfterShadowPass();

    DrawSkyPass();
    DrawWorldPass();
    DrawOverlayScreenPass();

    if (mPost.type != PostEffectType::None)
    {
        RenderTarget::Unbind();
        glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);
        DrawPostEffectPass();
    }

    DrawUIPass();
    DrawFadePass();
    EndFrame();
}

//=============================================================
// BuildFrameQueues
//=============================================================
void Renderer::BuildFrameQueues()
{
    mFrame.Clear();
    mBuckets.Clear();

    // frustum cull（今のまま：カメラVPでの可視判定）
    const Matrix4 vp = GetViewMatrix() * GetProjectionMatrix();
    const Frustum fr = BuildFrustumFromMatrix(vp);

    RenderQueue tmpRender;
    RenderQueue tmpShadow;

    for (auto* vc : mVisualComps)
    {
        if (!vc || !vc->IsVisible()) continue;

        // -----------------------------
        // (A) カメラ frustum cull（今のまま）
        // -----------------------------
        const VisualLayer layer = vc->GetLayer();
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
                        continue;
                    }
                }
            }
        }

        // -----------------------------
        // (B) Shadow items を先に積む（★統合）
        // -----------------------------
        if (vc->GetEnableShadow())
        {
            tmpShadow.Clear();
            vc->GatherShadowItems(tmpShadow);

            for (const auto& it : tmpShadow.Items())
            {
                // ★ここで pass=Shadow の RenderItem を積む想定
                const uint32_t idx = mFrame.Push(it);
                mBuckets.shadowCaster.push_back(idx);
            }
        }

        // -----------------------------
        // (C) 通常 items（World/UI/Overlay...）
        // -----------------------------
        tmpRender.Clear();
        vc->GatherRenderItems(tmpRender);

        for (const auto& it : tmpRender.Items())
        {
            const uint32_t idx = mFrame.Push(it);

            // ★もし RenderItems 側にも pass=Shadow が紛れたらここで拾う（安全弁）
            if (it.pass == RenderPass::Shadow)
            {
                mBuckets.shadowCaster.push_back(idx);
                continue;
            }

            // ここで “1回だけ分類”
            if (it.pass == RenderPass::UI || it.layer == VisualLayer::UI)
            {
                mBuckets.ui.push_back(idx);
                continue;
            }

            switch (it.layer)
            {
                case VisualLayer::Sky:
                    mBuckets.sky.push_back(idx);
                    break;

                case VisualLayer::OverlayScreen:
                    mBuckets.overlayScreen.push_back(idx);
                    break;

                case VisualLayer::Object3D:
                    if (it.blend == BlendMode::Opaque)
                        mBuckets.worldOpaque.push_back(idx);
                    else
                        mBuckets.worldTransparent.push_back(idx);
                    break;

                case VisualLayer::Effect3D:
                {
                    const bool isPre =
                        (it.type == RenderItemType::Sprite) ||
                        (it.type == RenderItemType::Debug);
                    if (isPre) mBuckets.effectPre.push_back(idx);
                    else       mBuckets.effectOverlay.push_back(idx);
                    break;
                }

                default:
                    // とりあえず Object3D 扱い
                    if (it.blend == BlendMode::Opaque)
                        mBuckets.worldOpaque.push_back(idx);
                    else
                        mBuckets.worldTransparent.push_back(idx);
                    break;
            }
        }
    }

    // -----------------------------
    // (D) Sort：bucket の index を並び替える（mFrame を参照）
    // -----------------------------
    SortBucket(mBuckets.sky);
    SortBucket(mBuckets.worldOpaque);
    SortBucket(mBuckets.effectPre);
    SortBucket(mBuckets.worldTransparent);
    SortBucket(mBuckets.effectOverlay);
    SortBucket(mBuckets.overlayScreen);
    SortBucket(mBuckets.ui);

    // ★Shadow は専用 sort が無難（今は no-op でもOK）
    SortBucket_Shadow(mBuckets.shadowCaster);
}

void Renderer::SortBucket(std::vector<uint32_t>& bucket)
{
    if (bucket.size() <= 1)
    {
        return;
    }

    auto& items = mFrame.items;

    std::stable_sort(
        bucket.begin(),
        bucket.end(),
        [&items](uint32_t ia, uint32_t ib)
        {
            // 範囲外は末尾へ（安全弁）
            const bool aValid = (ia < items.size());
            const bool bValid = (ib < items.size());
            if (aValid != bValid) return aValid;   // valid が先
            if (!aValid && !bValid) return false;  // 両方invalidなら順序維持

            const RenderItem& a = items[ia];
            const RenderItem& b = items[ib];

            // 1) RenderPass（enum順）
            if (a.pass != b.pass)
            {
                return a.pass < b.pass;
            }

            // 2) VisualLayer（enum順）
            if (a.layer != b.layer)
            {
                return a.layer < b.layer;
            }

            // 3) BlendMode：Opaque を先に（Z確定）
            const bool aOpaque = (a.blend == BlendMode::Opaque);
            const bool bOpaque = (b.blend == BlendMode::Opaque);
            if (aOpaque != bOpaque)
            {
                return aOpaque; // true(opaque) が先
            }

            // 4) DrawOrder
            if (a.drawOrder != b.drawOrder)
            {
                return a.drawOrder < b.drawOrder;
            }

            // 5) 完全一致：stable_sort で投入順維持
            return false;
        }
    );
}
void Renderer::SortBucket_Shadow(std::vector<uint32_t>& bucket)
{
    auto& items = mFrame.Items();

    std::stable_sort(
        bucket.begin(),
        bucket.end(),
        [&](uint32_t a, uint32_t b)
        {
            const RenderItem& A = items[a];
            const RenderItem& B = items[b];

            // 0) 念のため：Shadow以外が混ざってたら後ろへ
            const bool aShadow = (A.pass == RenderPass::Shadow);
            const bool bShadow = (B.pass == RenderPass::Shadow);
            if (aShadow != bShadow) return aShadow; // trueが先

            // 1) shader でまとめる（SetActive削減）
            if (A.shader.ptr != B.shader.ptr)
            {
                return A.shader.ptr < B.shader.ptr;
            }

            // 2) geometry でまとめる（VAO bind削減）
            if (A.type != RenderItemType::Particle) // Particleはshadow描かない想定なら無視でもOK
            {
                if (A.geometry.ptr != B.geometry.ptr)
                {
                    return A.geometry.ptr < B.geometry.ptr;
                }
            }

            // 3) Skinned の palette 有無で軽くまとめる（任意）
            const bool aSkinned = (A.type == RenderItemType::SkinnedMesh);
            const bool bSkinned = (B.type == RenderItemType::SkinnedMesh);
            if (aSkinned != bSkinned) return aSkinned; // skinned先（or逆でもOK）

            return false; // stable_sortに任せる
        }
    );
}

//=============================================================
// DrawToRenderTarget
//=============================================================
void Renderer::DrawToRenderTarget(const SceneCaptureRequest& req)
{
    if (!req.rt) return;

    ChangeDebugRTT();

    // ---- Save per-capture state ----
    const Matrix4 prevView = mViewMatrix;
    const Matrix4 prevProj = mProjectionMatrix;
    const Matrix4 prevInvV = mInvView;

    GLint prevFBO = 0;
    GLint prevVP[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevVP);

    // ---- Bind RT + viewport ----
    req.rt->Bind();
    glViewport(0, 0, (GLsizei)req.rt->GetWidth(), (GLsizei)req.rt->GetHeight());

    // ★Sky-only の場合は「色を毎回クリアしない」方が水面で破綻しにくい
    //   （必要ならここを req側のフラグにしてもいい）
    if (req.drawWorld || req.drawOverlay || req.drawUI)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else
    {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    // ---- Override camera ----
    mViewMatrix       = req.view;
    mProjectionMatrix = req.proj;
    mInvView          = req.view;
    mInvView.Invert();

    // ---- Shadow for this capture ----
    // ★Worldを描かないなら Shadow も不要（重い＆副作用の元）
    if (req.drawWorld)
    {
        RenderShadowPass();

        // RestoreAfterShadowPass() が画面VPに戻すので、RTに戻す
        RestoreAfterShadowPass();
        req.rt->Bind();
        glViewport(0, 0, (GLsizei)req.rt->GetWidth(), (GLsizei)req.rt->GetHeight());
    }
    else
    {
        // Sky-only のときも念のため、RT側の描画状態に寄せる
        glDisable(GL_STENCIL_TEST);
    }

    // ---- Draw scene into RT ----
    BuildFrameQueues();

    if (req.drawSky)     DrawSkyPass();
    if (req.drawWorld)   DrawWorldPass();
    if (req.drawOverlay) DrawOverlayScreenPass();
    if (req.drawUI)      DrawUIPass();

    DrawFadePass();

    // ---- Restore camera ----
    mViewMatrix       = prevView;
    mProjectionMatrix = prevProj;
    mInvView          = prevInvV;

    // ---- Restore FBO + viewport ----
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);

    ChangeDebugOnScreen();
}


} // namespace toy
