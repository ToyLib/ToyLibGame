//==============================================================================
// Renderer_DrawPass.cpp
//  - RenderQueue draw helpers
//  - GL state apply
//  - Shadow / World / UI / Post passes
//  - Draw(), BuildFrameQueues(), DrawToRenderTarget()
//==============================================================================

#include "Engine/Render/IRenderer.h"

// Engine / Render
#include "Engine/Render/LightingManager.h"
#include "Engine/Render/RenderTarget.h"
#include "Engine/Render/Shader.h"

// Geometry / Visual
#include "Asset/Geometry/VertexArray.h"
#include "Graphics/VisualComponent.h"

// Core / Physics
#include "Engine/Core/Actor.h"
#include "Physics/BoundingVolumeComponent.h"

// Utils
#include "Utils/FrustumUtil.h"

// GL
#include "glad/glad.h"

// Std
#include <algorithm>
#include <iostream>

namespace toy {

//==============================================================================
// Bucket draw helpers
//  - bucket は「mRenderQueue.Items() の index 配列」
//  - 各 Pass は bucket を走査して対応する RenderItem を描画する
//==============================================================================

void IRenderer::DrawBucket_World(const std::vector<uint32_t>& bucket)
{
    const auto& items = mRenderQueue.Items();

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size())
        {
            continue; // safety
        }

        const RenderItem& it = items[idx];

        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem_GL(it, RenderPass::World, -1);
    }
}

void IRenderer::DrawBucket_Shadow(const std::vector<uint32_t>& bucket, int cascadeIndex)
{
    if (bucket.empty())
    {
        return;
    }

    const auto& items = mRenderQueue.Items();

    for (uint32_t idx : bucket)
    {
        if (idx >= items.size())
        {
            continue; // safety
        }

        const RenderItem& it = items[idx];

        // safety: UI が混入していた場合は Shadow から除外
        if (it.pass == RenderPass::UI || it.layer == VisualLayer::UI)
        {
            continue;
        }

        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem_GL(it, RenderPass::Shadow, cascadeIndex);
    }
}

//==============================================================================
// GL state apply
//  - RenderItem が要求する描画状態を OpenGL に反映
//==============================================================================

void IRenderer::ApplyState_GL(const RenderItem& it)
{
    // depth test
    if (it.depthTest) glEnable(GL_DEPTH_TEST);
    else             glDisable(GL_DEPTH_TEST);

    // depth func
    if (it.type == RenderItemType::SkyDome) glDepthFunc(GL_LEQUAL);
    else                                   glDepthFunc(GL_LESS);

    // depth write
    glDepthMask(it.depthWrite ? GL_TRUE : GL_FALSE);

    // blend
    if (it.blend == BlendMode::Opaque)
    {
        glDisable(GL_BLEND);
    }
    else
    {
        glEnable(GL_BLEND);

        if (it.blend == BlendMode::Additive)
        {
            glBlendFunc(GL_ONE, GL_ONE);
        }
        else
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
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

    // color mask (default)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

//==============================================================================
// Helpers (cpp internal)
//==============================================================================

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
        {
            return false;
        }
    }

    return true;
}

inline void SetCommonUniforms(IRenderer& r,
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

inline void DrawDefaultGeometry_GL(IRenderer& r, const RenderItem& it)
{
    if (it.type == RenderItemType::Particle)
    {
        glBindVertexArray(it.gpuVAO);

        glDrawElementsInstanced(
            GL_TRIANGLES,
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
    {
        glDrawElements(mode, it.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    else
    {
        glDrawArrays(mode, 0, it.vertexCount);
    }

    r.AddDrawCall();
}

} // unnamed namespace

//==============================================================================
// DrawItem_GL
//==============================================================================

void IRenderer::DrawItem_GL(const RenderItem& it, RenderPass pass, int cascadeIndex)
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

//==============================================================================
// Frame begin/end
//==============================================================================

void IRenderer::BeginFrame()
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

void IRenderer::EndFrame()
{
    SDL_GL_SwapWindow(mWindow);
}

//==============================================================================
// Shadow pass
//==============================================================================

void IRenderer::RenderShadowPass()
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
            mShadowFar);

        Matrix4 lightVP = lightView * lightProj;
        mLightSpaceMatrix[i] = lightVP;

        // caster は BuildFrameQueues() 側で集約済み
        DrawBucket_Shadow(mBuckets.shadowCaster, i);
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
}

void IRenderer::RestoreAfterShadowPass()
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

//==============================================================================
// Individual passes
//==============================================================================

void IRenderer::DrawSkyPass()
{
    if (mBuckets.sky.empty()) return;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    DrawBucket_World(mBuckets.sky);

    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void IRenderer::DrawWorldPass()
{
    // 2) WORLD OPAQUE
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glDisable(GL_BLEND);

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

void IRenderer::DrawOverlayScreenPass()
{
    if (mBuckets.overlayScreen.empty()) return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawBucket_World(mBuckets.overlayScreen);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void IRenderer::DrawFadePass()
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

void IRenderer::DrawUIPass()
{
    if (mBuckets.ui.empty()) return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawBucket_World(mBuckets.ui);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void IRenderer::DrawPostEffectPass()
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
            sh->SetFloatUniform("uTime", SDL_GetTicks());
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

//==============================================================================
// Main Draw
//==============================================================================

void IRenderer::Draw()
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

//==============================================================================
// BuildFrameQueues
//  - VisualComponent から RenderItem を回収し、mRenderQueue に集約
//  - 同時に bucket に分類し、DrawPass では bucket を走査して描画する
//==============================================================================

void IRenderer::BuildFrameQueues()
{
    mRenderQueue.Clear();
    mBuckets.Clear();

    // メインカメラ用 frustum（通常描画用）
    const Matrix4 vp = GetViewMatrix() * GetProjectionMatrix();
    const Frustum cameraFrustum = BuildFrustumFromMatrix(vp);

    for (auto* vc : mVisualComps)
    {
        if (!vc || !vc->IsVisible())
        {
            continue;
        }

        //====================================================
        // (B) Shadow caster（★frustum cull しない）
        //  - payload も mRenderQueue に直接積まれるので消えない
        //====================================================
        if (vc->GetEnableShadow())
        {
            const uint32_t before = static_cast<uint32_t>(mRenderQueue.Items().size());

            vc->GatherShadowItems(mRenderQueue);

            const uint32_t after = static_cast<uint32_t>(mRenderQueue.Items().size());
            for (uint32_t i = before; i < after; ++i)
            {
                mBuckets.shadowCaster.push_back(i);
            }
        }

        //====================================================
        // (A) 通常描画だけ frustum cull
        //====================================================
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
                    if (!FrustumIntersectsAABB(cameraFrustum, aabb))
                    {
                        continue; // ★Shadowは上で積んでるので「通常描画」だけ止める
                    }
                }
            }
        }

        //====================================================
        // (C) 通常 items（World/UI/Overlay...）
        //  - payload も mRenderQueue に直接積まれるので消えない
        //====================================================
        const uint32_t before = static_cast<uint32_t>(mRenderQueue.Items().size());

        vc->GatherRenderItems(mRenderQueue);

        const uint32_t after = static_cast<uint32_t>(mRenderQueue.Items().size());
        const auto& items = mRenderQueue.Items();

        for (uint32_t i = before; i < after; ++i)
        {
            const RenderItem& it = items[i];

            // safety: RenderItems 側に Shadow が混ざってた場合
            if (it.pass == RenderPass::Shadow)
            {
                mBuckets.shadowCaster.push_back(i);
                continue;
            }

            // UI
            if (it.pass == RenderPass::UI || it.layer == VisualLayer::UI)
            {
                mBuckets.ui.push_back(i);
                continue;
            }

            switch (it.layer)
            {
                case VisualLayer::Sky:
                    mBuckets.sky.push_back(i);
                    break;

                case VisualLayer::OverlayScreen:
                    mBuckets.overlayScreen.push_back(i);
                    break;

                case VisualLayer::Object3D:
                    if (it.blend == BlendMode::Opaque)
                        mBuckets.worldOpaque.push_back(i);
                    else
                        mBuckets.worldTransparent.push_back(i);
                    break;

                case VisualLayer::Effect3D:
                {
                    const bool isPre =
                        (it.type == RenderItemType::Sprite) ||
                        (it.type == RenderItemType::Debug);

                    if (isPre) mBuckets.effectPre.push_back(i);
                    else       mBuckets.effectOverlay.push_back(i);
                    break;
                }

                default:
                    if (it.blend == BlendMode::Opaque)
                        mBuckets.worldOpaque.push_back(i);
                    else
                        mBuckets.worldTransparent.push_back(i);
                    break;
            }
        }
    }

    // Sort
    SortBucket(mBuckets.sky);
    SortBucket(mBuckets.worldOpaque);
    SortBucket(mBuckets.effectPre);
    SortBucket(mBuckets.worldTransparent);
    SortBucket(mBuckets.effectOverlay);
    SortBucket(mBuckets.overlayScreen);
    SortBucket(mBuckets.ui);

    SortBucket_Shadow(mBuckets.shadowCaster);
}

//==============================================================================
// Bucket sort
//==============================================================================

void IRenderer::SortBucket(std::vector<uint32_t>& bucket)
{
    if (bucket.size() <= 1)
    {
        return;
    }

    auto& items = mRenderQueue.Items();

    std::stable_sort(
        bucket.begin(),
        bucket.end(),
        [&items](uint32_t ia, uint32_t ib)
        {
            // safety: 範囲外は末尾へ
            const bool aValid = (ia < items.size());
            const bool bValid = (ib < items.size());
            if (aValid != bValid) return aValid;
            if (!aValid && !bValid) return false;

            const RenderItem& a = items[ia];
            const RenderItem& b = items[ib];

            // 1) RenderPass（enum順）
            if (a.pass != b.pass) return a.pass < b.pass;

            // 2) VisualLayer（enum順）
            if (a.layer != b.layer) return a.layer < b.layer;

            // 3) BlendMode：Opaque を先に（Z 確定）
            const bool aOpaque = (a.blend == BlendMode::Opaque);
            const bool bOpaque = (b.blend == BlendMode::Opaque);
            if (aOpaque != bOpaque) return aOpaque;

            // 4) DrawOrder
            if (a.drawOrder != b.drawOrder) return a.drawOrder < b.drawOrder;

            // 5) 完全一致：stable_sort で投入順維持
            return false;
        }
    );
}

void IRenderer::SortBucket_Shadow(std::vector<uint32_t>& bucket)
{
    auto& items = mRenderQueue.Items();

    std::stable_sort(
        bucket.begin(),
        bucket.end(),
        [&](uint32_t a, uint32_t b)
        {
            const RenderItem& A = items[a];
            const RenderItem& B = items[b];

            // 0) safety: Shadow 以外が混入していたら後ろへ
            const bool aShadow = (A.pass == RenderPass::Shadow);
            const bool bShadow = (B.pass == RenderPass::Shadow);
            if (aShadow != bShadow) return aShadow;

            // 1) shader でまとめる（SetActive 削減）
            if (A.shader.ptr != B.shader.ptr) return A.shader.ptr < B.shader.ptr;

            // 2) geometry でまとめる（VAO bind 削減）
            if (A.type != RenderItemType::Particle)
            {
                if (A.geometry.ptr != B.geometry.ptr)
                {
                    return A.geometry.ptr < B.geometry.ptr;
                }
            }

            // 3) Skinned をまとめる（任意）
            const bool aSkinned = (A.type == RenderItemType::SkinnedMesh);
            const bool bSkinned = (B.type == RenderItemType::SkinnedMesh);
            if (aSkinned != bSkinned) return aSkinned;

            return false; // stable_sort に任せる（投入順維持）
        }
    );
}

//==============================================================================
// DrawToRenderTarget
//==============================================================================

void IRenderer::DrawToRenderTarget(const SceneCaptureRequest& req)
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

    // Sky-only の場合は「色を毎回クリアしない」方が水面で破綻しにくい
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

    mInvView = req.view;
    mInvView.Invert();

    // ---- Shadow for this capture ----
    if (req.drawWorld)
    {
        RenderShadowPass();

        // RestoreAfterShadowPass() が画面 VP に戻すので、RT に戻す
        RestoreAfterShadowPass();
        req.rt->Bind();
        glViewport(0, 0, (GLsizei)req.rt->GetWidth(), (GLsizei)req.rt->GetHeight());
    }
    else
    {
        // Sky-only のときも念のため、RT 側の描画状態に寄せる
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
