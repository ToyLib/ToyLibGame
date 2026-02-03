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
void Renderer::DrawRenderQueue_World(const RenderQueue& queue)
{
    for (const auto& it : queue.Items())
    {
        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem_GL(it, RenderPass::World, -1);
    }
}

void Renderer::DrawRenderQueue_Shadow(const RenderQueue& queue, int cascadeIndex)
{
    for (const auto& it : queue.Items())
    {
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

        Frustum shadowFrustum = BuildFrustumFromMatrix(lightVP);

        RenderQueue queue;

        for (auto* vc : mVisualComps)
        {
            if (!vc || !vc->GetEnableShadow() || !vc->IsVisible())
            {
                continue;
            }

            Actor* owner = vc->GetOwner();
            if (owner)
            {
                auto bv = owner->GetComponent<BoundingVolumeComponent>();
                if (bv)
                {
                    Cube aabb = bv->GetWorldAABB();
                    if (!FrustumIntersectsAABB(shadowFrustum, aabb))
                    {
                        continue;
                    }
                }
            }

            vc->GatherShadowItems(queue);
        }

        DrawRenderQueue_Shadow(queue, i);
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
    if (mQ_Sky.Items().empty()) return;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    mQ_Sky.Sort();
    DrawRenderQueue_World(mQ_Sky);

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

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // 3) WORLD EFFECT (PRE)
    {
        RenderQueue effectPreQueue;
        for (const auto& it : mQ_Effect3D.Items())
        {
            const bool isPre =
                (it.type == RenderItemType::Sprite) ||
                (it.type == RenderItemType::Debug);
            if (isPre) effectPreQueue.Push(it);
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        effectPreQueue.Sort();
        DrawRenderQueue_World(effectPreQueue);

        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    // 4) WORLD TRANSPARENT
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

        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    // 5) WORLD EFFECT (OVERLAY)
    {
        RenderQueue effectOverlayQueue;
        for (const auto& it : mQ_Effect3D.Items())
        {
            const bool isOverlay =
                (it.type == RenderItemType::Billboard) ||
                (it.type == RenderItemType::Particle);
            if (isOverlay) effectOverlayQueue.Push(it);
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        effectOverlayQueue.Sort();
        DrawRenderQueue_World(effectOverlayQueue);

        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }
}

void Renderer::DrawOverlayScreenPass()
{
    if (mQ_OverlayScreen.Items().empty()) return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mQ_OverlayScreen.Sort();
    DrawRenderQueue_World(mQ_OverlayScreen);

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
    if (mQ_UI.Items().empty()) return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mQ_UI.Sort();
    DrawRenderQueue_World(mQ_UI);

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
    mQ_Sky.Clear();
    mQ_Object3D.Clear();
    mQ_Effect3D.Clear();
    mQ_OverlayScreen.Clear();
    mQ_UI.Clear();

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
