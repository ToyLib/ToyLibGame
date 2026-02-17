// Render/DrawDispatch.cpp
#include "Render/RenderItem.h"

#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"
#include "Render/LightingManager.h"
#include "Render/IRenderer.h"
#include "Render/GL/GLShader.h"

namespace toy {

//============================================================
// Sprite
//============================================================
static bool DispatchSprite(IRenderer& r,
                           const RenderItem& it,
                           RenderPass pass,
                           int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.pipeline.ptrGLShader;
    if (!sh)
    {
        return true;
    }

    // Payload優先（なければ旧メンバ）
    Vector3 color = Vector3::Zero;
    float   alpha = 1.0f;

    // 例：payloadIndexが有効なときだけ使う、みたいなルールにする
    // （0番が常に有効payloadになる設計なら、この条件は別のフラグに）
    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const SpritePayload& sp = r.GetSpritePayload(it.payloadIndex);
        color = sp.color;
        alpha = sp.alpha;
    }

    sh->SetVectorUniform("uSpriteColor", color);
    sh->SetFloatUniform ("uSpriteAlpha", alpha);

    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);
        sh->SetTextureUniform("uTexture", it.textureUnit);
        sh->SetBooleanUniform("uUseTexture", true);
    }
    else
    {
        sh->SetBooleanUniform("uUseTexture", false);
    }

    return false;
}
//============================================================
// Mesh
//============================================================
static bool DispatchMesh(IRenderer& r,
                         const RenderItem& it,
                         RenderPass pass,
                         int)
{
    if (pass != RenderPass::World)
    {
        return false;
    }

    auto* sh = it.pipeline.ptrGLShader;
    if (!sh)
    {
        return true;
    }

    if (r.GetLightingManager())
    {
        const Matrix4 view = r.GetViewMatrix();
        r.GetLightingManager()->ApplyToShader(sh, view);
    }

    if (auto sm0 = r.GetShadowMapTexture(0)) sm0->SetActive(6);
    if (auto sm1 = r.GetShadowMapTexture(1)) sm1->SetActive(7);

    sh->SetTextureUniform("uShadowMap0", 6);
    sh->SetTextureUniform("uShadowMap1", 7);

    sh->SetMatrixUniform("uLightViewProj0", r.GetLightSpaceMatrix(0));
    sh->SetMatrixUniform("uLightViewProj1", r.GetLightSpaceMatrix(1));

    sh->SetFloatUniform("uCascadeSplit0", r.GetCascadeSplit0());
    sh->SetFloatUniform("uCascadeBlend",  r.GetCascadeBlend());
    sh->SetFloatUniform("uShadowBias",    0.005f);

    sh->SetBooleanUniform("uUseToon", it.toon);

    // ----------------------------
    // Material（輪郭の override color 対応）
    // ----------------------------
    if (it.material.ptr)
    {
        if (it.overrideColor)
        {
            it.material.ptr->SetOverrideColor(true, it.overrideColorValue);
            it.material.ptr->BindToShader(sh, 0);
            it.material.ptr->SetOverrideColor(false, Vector3(0.0f, 0.0f, 0.0f));
        }
        else
        {
            it.material.ptr->BindToShader(sh, 0);
        }
    }

    return false;
}

//============================================================
// SkinnedMesh
//============================================================
static bool DispatchSkinnedMesh(IRenderer& r,
                                const RenderItem& it,
                                RenderPass pass,
                                int)
{
    auto* sh = it.pipeline.ptrGLShader;
    if (!sh)
    {
        return true; // 何もしない（安全弁）
    }

    //============================================================
    // Shadow pass（通常は ShadowSkinned 側で処理する想定）
    //============================================================
    if (pass == RenderPass::Shadow)
    {
        sh->SetMatrixUniform("uWorldTransform", it.world);

        if (it.matrixPalette && it.paletteCount > 0)
        {
            sh->SetMatrixUniforms("uMatrixPalette",
                                  it.matrixPalette,
                                  static_cast<unsigned int>(it.paletteCount));
        }
        return false;
    }

    //============================================================
    // World pass
    //============================================================
    if (pass != RenderPass::World)
    {
        return false;
    }

    sh->SetMatrixUniform("uWorldTransform", it.world);
    sh->SetMatrixUniform("uViewProj",       it.viewProj);

    // ライティング
    if (r.GetLightingManager())
    {
        const Matrix4 view = r.GetViewMatrix();
        r.GetLightingManager()->ApplyToShader(sh, view);
    }

    // シャドウ（輪郭は不要なので overrideColor でスキップ）
    if (!it.overrideColor)
    {
        if (auto sm0 = r.GetShadowMapTexture(0)) sm0->SetActive(6);
        if (auto sm1 = r.GetShadowMapTexture(1)) sm1->SetActive(7);

        sh->SetTextureUniform("uShadowMap0", 6);
        sh->SetTextureUniform("uShadowMap1", 7);

        sh->SetMatrixUniform("uLightViewProj0", r.GetLightSpaceMatrix(0));
        sh->SetMatrixUniform("uLightViewProj1", r.GetLightSpaceMatrix(1));

        sh->SetFloatUniform("uCascadeSplit0", r.GetCascadeSplit0());
        sh->SetFloatUniform("uCascadeBlend",  r.GetCascadeBlend());
        sh->SetFloatUniform("uShadowBias",    0.005f);
    }

    sh->SetBooleanUniform("uUseToon", it.toon);

    // Material bind（overrideColor 反映→bind→戻す）
    if (it.material.ptr)
    {
        if (it.overrideColor)
        {
            it.material.ptr->SetOverrideColor(true, it.overrideColorValue);
        }

        it.material.ptr->BindToShader(sh, 0);

        if (it.overrideColor)
        {
            it.material.ptr->SetOverrideColor(false, Vector3(0.0f, 0.0f, 0.0f));
        }
    }

    // matrix palette
    if (it.matrixPalette && it.paletteCount > 0)
    {
        sh->SetMatrixUniforms("uMatrixPalette",
                              it.matrixPalette,
                              static_cast<unsigned int>(it.paletteCount));
    }

    return false; // DrawDefaultGeometry に流す
}

//============================================================
// Billboard
//============================================================
static bool DispatchBillboard(IRenderer& r,
                              const RenderItem& it,
                              RenderPass pass,
                              int)
{
    if (pass == RenderPass::Shadow)
        return true;

    auto* sh = it.pipeline.ptrGLShader;
    if (!sh)
        return true;

    Vector3 color = Vector3(1.0f, 1.0f, 1.0f);
    float   alpha = 1.0f;

    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const BillboardPayload& bp = r.GetBillboardPayload(it.payloadIndex);
        color = bp.color;
        alpha = bp.alpha;
    }

    // もし billboard shader が色/αを受けるなら
    sh->SetVectorUniform("uSpriteColor", color);
    sh->SetFloatUniform ("uSpriteAlpha", alpha);

    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);
        sh->SetTextureUniform("uTexture", it.textureUnit);
        sh->SetBooleanUniform("uUseTexture", true);
    }
    else
    {
        sh->SetBooleanUniform("uUseTexture", false);
    }

    return false;
}

//============================================================
// GPUParticle
//============================================================
static bool DispatchParticle(IRenderer& r,
                             const RenderItem& it,
                             RenderPass pass,
                             int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.pipeline.ptrGLShader;
    if (!sh)
    {
        return true;
    }

    Vector3 camRight(1,0,0);
    Vector3 camUp   (0,1,0);
    float   lifeMax = 1.0f;
    float   size    = 1.0f;

    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const ParticlePayload& pp = r.GetParticlePayload(it.payloadIndex);
        camRight = pp.cameraRight;
        camUp    = pp.cameraUp;
        lifeMax  = pp.particleLifeMax;
        size     = pp.particleSize;
    }

    sh->SetVectorUniform("uCameraRight", camRight);
    sh->SetVectorUniform("uCameraUp",    camUp);
    sh->SetFloatUniform ("uLifeMax",      lifeMax);
    sh->SetFloatUniform ("uSize",         size);

    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);
        sh->SetTextureUniform("uTexture", it.textureUnit);
    }

    return false; // instanced draw に流す
}

//============================================================
// SkyDome（完全自前描画）
//============================================================
static bool DispatchSkyDome(IRenderer& r,
                            const RenderItem& it,
                            RenderPass pass,
                            int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.pipeline.ptrGLShader;
    if (!sh || !it.geometry.ptr)
    {
        return true;
    }

    // payload（無ければデフォルトで安全）
    SkyDomePayload sky {};
    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        sky = r.GetSkyDomePayload(it.payloadIndex);
    }

    if (sky.useMVP)
    {
        sh->SetMatrixUniform("uMVP", sky.mvp);
    }

    sh->SetFloatUniform("uTime",        sky.skyTime);
    sh->SetIntUniform  ("uWeatherType", sky.skyWeatherType);
    sh->SetFloatUniform("uTimeOfDay",   sky.skyTimeOfDay);

    sh->SetVectorUniform("uSunDir",        sky.skySunDir);
    sh->SetVectorUniform("uMoonDir",       sky.skyMoonDir);
    sh->SetVectorUniform("uRawSkyColor",   sky.skyRawSkyColor);
    sh->SetVectorUniform("uRawCloudColor", sky.skyRawCloudColor);

    // もし fog uniform があるなら
    // sh->SetVectorUniform("uFogColor", sky.skyFogColor);
    // sh->SetFloatUniform ("uFogDensity", sky.skyFogDensity);

    it.geometry.ptr->SetActive();
    glDrawElements(GL_TRIANGLES, it.indexCount, GL_UNSIGNED_INT, nullptr);

    r.AddDrawCall();
    return true; // ここで描いた
}
//============================================================
// Overlay（フルスクリーン系）
//============================================================
static bool DispatchOverlay(IRenderer& r,
                            const RenderItem& it,
                            RenderPass pass,
                            int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.pipeline.ptrGLShader;
    if (!sh || !it.geometry.ptr)
    {
        return true;
    }

    // ---- payload ----
    OverlayPayload op {};
    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        // Renderer側に GetOverlayPayload を持たせてる前提なら r.GetOverlayPayload()
        // RenderQueueに持たせてる前提なら r.GetRenderQueue().GetOverlayPayload() 等に差し替えてOK
        op = r.GetOverlayPayload(it.payloadIndex);
    }

    sh->SetFloatUniform("uTime", op.time);

    sh->SetFloatUniform("uRainAmount", op.rainAmount);
    sh->SetFloatUniform("uFogAmount",  op.fogAmount);
    sh->SetFloatUniform("uSnowAmount", op.snowAmount);

    sh->SetVector2Uniform("uResolution", op.resolution);

    sh->SetFloatUniform  ("uFlareIntensity", op.flareIntensity);
    sh->SetVector2Uniform("uSunPos",         op.sunPos);
    sh->SetVectorUniform ("uFlareColor",     op.flareColor);

    it.geometry.ptr->SetActive();
    glDrawElements(GL_TRIANGLES, it.indexCount, GL_UNSIGNED_INT, nullptr);

    r.AddDrawCall();
    return true;
}
//============================================================
// Debug
//============================================================
static bool DispatchDebug(IRenderer& r,
                          const RenderItem& it,
                          RenderPass pass,
                          int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.pipeline.ptrGLShader;
    if (!sh)
    {
        return true;
    }

    Vector3 color = Vector3(1.0f, 1.0f, 1.0f);
    float   alpha = 1.0f;

    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const DebugPayload& dp = r.GetDebugPayload(it.payloadIndex);
        color = dp.color;
        alpha = dp.alpha;
    }

    sh->SetVectorUniform("uSolColor", color);
    // sh->SetFloatUniform("uAlpha", alpha); // 必要なら

    return false;
}
//============================================================
// Surface
//============================================================
static bool DispatchSurface(IRenderer& r,
                            const RenderItem& it,
                            RenderPass pass,
                            int)
{
    if (pass != RenderPass::World)
    {
        return true;
    }

    auto* sh = it.pipeline.ptrGLShader;
    if (!sh)
    {
        return true;
    }

    sh->SetActive();

    // 行列（Surface用は分解してる前提）
    sh->SetMatrixUniform("uWorld", it.world);
    sh->SetMatrixUniform("uView",  r.GetViewMatrix());
    sh->SetMatrixUniform("uProj",  r.GetProjectionMatrix());

    // payload
    bool    flipX   = false;
    bool    flipY   = false;
    float   opacity = 1.0f;
    Vector3 tint    = Vector3(1.0f, 1.0f, 1.0f);
    int     mode    = 0;
    float   time    = 0.0f;
    float  scanlineStrength = 1.0f;

    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const SurfacePayload& sp = r.GetSurfacePayload(it.payloadIndex);
        flipX   = sp.flipX;
        flipY   = sp.flipY;
        opacity = sp.opacity;
        tint    = sp.tint;
        mode    = sp.mode;
        time    = sp.time;
        scanlineStrength = sp.scanlineStrength;
    }

    sh->SetBooleanUniform("uFlipX",   flipX);
    sh->SetBooleanUniform("uFlipY",   flipY);
    sh->SetFloatUniform  ("uOpacity", opacity);
    sh->SetVectorUniform ("uTint",    tint);
    sh->SetIntUniform    ("uMode",    mode);
    sh->SetFloatUniform  ("uTime",    time);
    sh->SetFloatUniform  ("uScanlineStrength", scanlineStrength);

    // テクスチャ
    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);
        sh->SetIntUniform("uSurfaceTex", it.textureUnit);
    }

    return false; // DrawDefaultGeometry に流す
}

//============================================================
// Dispatch selector
//============================================================
RenderItem::DispatchFn GetDispatch(RenderItemType type)
{
    switch (type)
    {
        case RenderItemType::Sprite:      return &DispatchSprite;
        case RenderItemType::Mesh:        return &DispatchMesh;
        case RenderItemType::SkinnedMesh: return &DispatchSkinnedMesh;
        case RenderItemType::Billboard:   return &DispatchBillboard;
        case RenderItemType::Particle:    return &DispatchParticle;
        case RenderItemType::SkyDome:     return &DispatchSkyDome;
        case RenderItemType::Overlay:     return &DispatchOverlay;
        case RenderItemType::Debug:       return &DispatchDebug;
        case RenderItemType::Surface:     return &DispatchSurface;
        default:                          return nullptr;
    }
}

} // namespace toy
