// Engine/Render/RenderItem.cpp
#include "Engine/Render/RenderItem.h"

#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"
#include "Engine/Render/LightingManager.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"

namespace toy {

//============================================================
// Sprite
//============================================================
static bool DispatchSprite(Renderer&,
                           const RenderItem& it,
                           RenderPass pass,
                           int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.shader.ptr;
    if (!sh)
    {
        return true;
    }

    sh->SetVectorUniform("uSpriteColor", it.color);
    sh->SetFloatUniform ("uSpriteAlpha", it.alpha);

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
static bool DispatchMesh(Renderer& r,
                         const RenderItem& it,
                         RenderPass pass,
                         int)
{
    if (pass != RenderPass::World)
    {
        return false;
    }

    auto* sh = it.shader.ptr;
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
static bool DispatchSkinnedMesh(Renderer& r,
                                const RenderItem& it,
                                RenderPass pass,
                                int)
{
    auto* sh = it.shader.ptr;
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
static bool DispatchBillboard(Renderer&,
                              const RenderItem& it,
                              RenderPass pass,
                              int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.shader.ptr;
    if (!sh)
    {
        return true;
    }

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
static bool DispatchParticle(Renderer&,
                             const RenderItem& it,
                             RenderPass pass,
                             int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.shader.ptr;
    if (!sh)
    {
        return true;
    }

    sh->SetVectorUniform("uCameraRight", it.cameraRight);
    sh->SetVectorUniform("uCameraUp",    it.cameraUp);
    sh->SetFloatUniform ("uLifeMax",     it.particleLifeMax);
    sh->SetFloatUniform ("uSize",        it.particleSize);

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
// SkyDome（完全自前描画）
//============================================================
static bool DispatchSkyDome(Renderer& r,
                            const RenderItem& it,
                            RenderPass pass,
                            int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.shader.ptr;
    if (!sh || !it.geometry.ptr)
    {
        return true;
    }

    if (it.useMVP)
    {
        sh->SetMatrixUniform("uMVP", it.mvp);
    }

    sh->SetFloatUniform("uTime",        it.skyTime);
    sh->SetIntUniform  ("uWeatherType", it.skyWeatherType);
    sh->SetFloatUniform("uTimeOfDay",   it.skyTimeOfDay);

    sh->SetVectorUniform("uSunDir",        it.skySunDir);
    sh->SetVectorUniform("uMoonDir",       it.skyMoonDir);
    sh->SetVectorUniform("uRawSkyColor",   it.skyRawSkyColor);
    sh->SetVectorUniform("uRawCloudColor", it.skyRawCloudColor);

    it.geometry.ptr->SetActive();
    glDrawElements(GL_TRIANGLES, it.indexCount, GL_UNSIGNED_INT, nullptr);

    r.AddDrawCall();
    r.AddDrawObject();
    return true; // ここで描いたので Default を抑止
}

//============================================================
// Overlay（フルスクリーン系）
//============================================================
static bool DispatchOverlay(Renderer& r,
                            const RenderItem& it,
                            RenderPass pass,
                            int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.shader.ptr;
    if (!sh || !it.geometry.ptr)
    {
        return true;
    }

    sh->SetFloatUniform("uTime", it.overlayTime);

    sh->SetFloatUniform("uRainAmount", it.overlayRainAmount);
    sh->SetFloatUniform("uFogAmount",  it.overlayFogAmount);
    sh->SetFloatUniform("uSnowAmount", it.overlaySnowAmount);

    sh->SetVector2Uniform("uResolution", it.overlayResolution);

    sh->SetFloatUniform  ("uFlareIntensity", it.overlayFlareIntensity);
    sh->SetVector2Uniform("uSunPos",         it.overlaySunPos);
    sh->SetVectorUniform ("uFlareColor",     it.overlayFlareColor);

    it.geometry.ptr->SetActive();
    glDrawElements(GL_TRIANGLES, it.indexCount, GL_UNSIGNED_INT, nullptr);

    r.AddDrawCall();
    r.AddDrawObject();
    return true;
}

//============================================================
// Debug
//============================================================
static bool DispatchDebug(Renderer&,
                          const RenderItem& it,
                          RenderPass pass,
                          int)
{
    if (pass == RenderPass::Shadow)
    {
        return true;
    }

    auto* sh = it.shader.ptr;
    if (!sh)
    {
        return true;
    }

    sh->SetVectorUniform("uSolColor", it.color);
    return false;
}

//============================================================
// Surface
//============================================================
static bool DispatchSurface(Renderer& r,
                            const RenderItem& it,
                            RenderPass pass,
                            int)
{
    if (pass != RenderPass::World)
    {
        return true; // 描かない
    }

    auto* sh = it.shader.ptr;
    if (!sh)
    {
        return true;
    }

    sh->SetActive();

    // 行列（Surface用は分解してる前提）
    sh->SetMatrixUniform("uWorld", it.world);
    sh->SetMatrixUniform("uView",  r.GetViewMatrix());
    sh->SetMatrixUniform("uProj",  r.GetProjectionMatrix());

    // パラメータ
    sh->SetBooleanUniform("uFlipX",   it.surfaceFlipX);
    sh->SetBooleanUniform("uFlipY",   it.surfaceFlipY);
    sh->SetFloatUniform  ("uOpacity", it.surfaceOpacity);
    sh->SetVectorUniform ("uTint",    it.surfaceTint);
    sh->SetIntUniform    ("uMode",    it.surfaceMode);
    sh->SetFloatUniform  ("uTime",    it.time);
    sh->SetFloatUniform  ("uScanlineStrength", it.scanlineStrength);
    

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
