#include "Render/GL/GLRenderer.h"
#include "Render/GL/GLRenderTarget.h"

#include "Render/LightingManager.h"
#include "Render/GL/GLShader.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"
#include "Render/GL/UniformNamesGL.h"

// GL
#include "glad/glad.h"

namespace toy {

//==============================================================================
// GL state apply
//  - RenderItem が要求する描画状態を OpenGL に反映
//==============================================================================

void GLRenderer::ApplyState(const RenderItem& it)
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
    auto* sh = it.pipeline.ptrGLShader;
    if (!sh) return;

    using namespace toy::glsl;

    //========================================================
    // World/UI : contract(v1)
    //========================================================
    if (pass == RenderPass::World || pass == RenderPass::UI)
    {
        // v1 contract
        sh->SetMatrixUniform(Scene::ViewProj, it.viewProj);
        sh->SetMatrixUniform(Object::World,   it.world);

        // (Optional) legacy fallback (消したいならここを後で削除)
        sh->SetMatrixUniform(Legacy::ViewProj,       it.viewProj);
        sh->SetMatrixUniform(Legacy::WorldTransform, it.world);
        return;
    }

    //========================================================
    // Shadow : contract(v1) + legacy fallback
    //========================================================
    if (pass == RenderPass::Shadow)
    {
        const Matrix4 lightVP = r.GetLightSpaceMatrix(cascadeIndex);

        // v1 contract
        sh->SetMatrixUniform(Object::World, it.world);

        // ★どっちを参照してもOKにする（暫定だが強い）
        sh->SetMatrixUniform(Scene::LightVP0, lightVP);
        sh->SetMatrixUniform(Scene::LightVP1, lightVP);

        // legacy も同様
        sh->SetMatrixUniform(Legacy::WorldTransform,   it.world);
        sh->SetMatrixUniform(Legacy::LightSpaceMatrix, lightVP);
        return;
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
// DrawItem
//==============================================================================

void GLRenderer::DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    if (!ValidateGeometryForDraw(it)) return;

    ApplyState(it);

    if (!it.pipeline.ptrGLShader) return;
    it.pipeline.ptrGLShader->SetActive();

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

bool GLRenderer::BeginFrame()
{
    const bool usePost = (mPost.type != PostEffectType::None);

    if (usePost)
    {
        if (!mSceneRT ||
            mSceneRT->GetWidth()  != (int)mScreenWidth ||
            mSceneRT->GetHeight() != (int)mScreenHeight)
        {
            mSceneRT = std::make_shared<GLRenderTarget>();
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
    
    return true;
}

void GLRenderer::EndFrame()
{
    SDL_GL_SwapWindow(mWindow);
}

//==============================================================================
// Shadow pass
//==============================================================================

void GLRenderer::DrawShadowPass()
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
void GLRenderer::RestoreAfterShadowPass()
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

void GLRenderer::DrawSkyPass()
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


void GLRenderer::DrawWorldPass()
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
void GLRenderer::DrawOverlayScreenPass()
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

void GLRenderer::DrawFadePass()
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
void GLRenderer::DrawUIPass()
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

void GLRenderer::DrawPostEffectPass()
{
    if (!mSceneRT) return;

    auto sceneTex = mSceneRT->GetColorTexture();
    if (!sceneTex) return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    auto sh = GetShader("PostEffect");
    if (!sh)
    {
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        return;
    }

    using namespace toy::glsl;

    sh->SetActive();

    // input texture
    sceneTex->SetActive(0);
    sh->SetTextureUniform(Post::SceneTex, 0);

    const float timeSec = (float)SDL_GetTicks() * 0.001f;

    switch (mPost.type)
    {
        case PostEffectType::None:
        case PostEffectType::Sepia:
        case PostEffectType::CRT:
        {
            sh->SetIntUniform  (Post::PostType,   (int)mPost.type);
            sh->SetFloatUniform(Post::Intensity,  mPost.intensity);
            sh->SetFloatUniform(Post::Time,       timeSec);
            sh->SetIntUniform  (Post::FlipY,      0);
            sh->SetIntUniform  (Post::UsePaperTex,0);
            break;
        }

        case PostEffectType::FeilyLand:
        {
            sh->SetIntUniform  (Post::PostType,   3);
            sh->SetFloatUniform(Post::Intensity,  1.0f);
            sh->SetFloatUniform(Post::Time,       timeSec);
            sh->SetIntUniform  (Post::FlipY,      0);
            sh->SetIntUniform  (Post::UsePaperTex,0);
            break;
        }

        case PostEffectType::Watercolor:
        {
            sh->SetIntUniform  (Post::PostType,   4);
            sh->SetFloatUniform(Post::Intensity,  1.0f);
            sh->SetFloatUniform(Post::Time,       timeSec);
            sh->SetIntUniform  (Post::FlipY,      0);

            sh->SetIntUniform(Post::UsePaperTex, 1);
            if (mPost.paperTex)
            {
                mPost.paperTex->SetActive(1);
                sh->SetTextureUniform(Post::PaperTex, 1);
            }
            else
            {
                sh->SetTextureUniform(Post::PaperTex, 0);
            }
            break;
        }
    }

    mFullScreenQuad->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

//==============================================================================
// DrawToRenderTarget
//==============================================================================

void GLRenderer::DrawToRenderTarget(const SceneCaptureRequest& req)
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
/*
    // ---- Shadow for this capture ----
    if (req.drawWorld)
    {
        DrawShadowPass();

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
*/
    // ---- Draw scene into RT ----
    BuildFrameQueues();

    if (req.drawSky)     DrawSkyPass();
    if (req.drawWorld)   DrawWorldPass();
    if (req.drawOverlay) DrawOverlayScreenPass();
    if (req.drawUI)      DrawUIPass();

    //DrawFadePass();

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
