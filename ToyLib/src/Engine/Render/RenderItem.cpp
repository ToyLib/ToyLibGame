// Engine/Render/RenderItem.cpp
#include "Engine/Render/RenderItem.h"

#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Render/Shader.h"

namespace toy {


//============================================================
// Sprite
//============================================================
bool DispatchSprite(Renderer& r,
                    const RenderItem& it,
                    RenderPass pass,
                    int)
{
    if (pass == RenderPass::Shadow)
        return true;

    auto* sh = it.shader.ptr;
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
bool DispatchMesh(Renderer& r,
                  const RenderItem& it,
                  RenderPass pass,
                  int)
{
    if (pass != RenderPass::World)
        return false;

    auto* sh = it.shader.ptr;

    if (r.GetLightingManager())
    {
        Matrix4 view = r.GetViewMatrix();
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

    if (it.material.ptr)
        it.material.ptr->BindToShader(sh, 0);

    return false;
}

//============================================================
// SkinnedMesh
//============================================================
bool DispatchSkinnedMesh(Renderer& r,
                         const RenderItem& it,
                         RenderPass pass,
                         int)
{
    auto* sh = it.shader.ptr;

    if (pass == RenderPass::World)
    {
        if (r.GetLightingManager())
        {
            Matrix4 view = r.GetViewMatrix();
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

        if (it.material.ptr)
            it.material.ptr->BindToShader(sh, 0);
    }

    if (it.matrixPalette && it.paletteCount > 0)
    {
        sh->SetMatrixUniforms("uMatrixPalette",
                              it.matrixPalette,
                              static_cast<unsigned int>(it.paletteCount));
    }

    return false;
}

//============================================================
// Billboard
//============================================================
bool DispatchBillboard(Renderer&,
                       const RenderItem& it,
                       RenderPass pass,
                       int)
{
    if (pass == RenderPass::Shadow)
        return true;

    auto* sh = it.shader.ptr;

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
bool DispatchGPUParticle(Renderer&,
                         const RenderItem& it,
                         RenderPass pass,
                         int)
{
    if (pass == RenderPass::Shadow)
        return true;

    auto* sh = it.shader.ptr;

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
bool DispatchSkyDome(Renderer& r,
                     const RenderItem& it,
                     RenderPass pass,
                     int)
{
    if (pass == RenderPass::Shadow)
        return true;

    auto* sh = it.shader.ptr;

    if (it.useMVP)
        sh->SetMatrixUniform("uMVP", it.mvp);

    sh->SetFloatUniform("uTime", it.skyTime);
    sh->SetIntUniform  ("uWeatherType", it.skyWeatherType);
    sh->SetFloatUniform("uTimeOfDay", it.skyTimeOfDay);

    sh->SetVectorUniform("uSunDir",        it.skySunDir);
    sh->SetVectorUniform("uMoonDir",       it.skyMoonDir);
    sh->SetVectorUniform("uRawSkyColor",   it.skyRawSkyColor);
    sh->SetVectorUniform("uRawCloudColor", it.skyRawCloudColor);

    it.geometry.ptr->SetActive();
    glDrawElements(GL_TRIANGLES, it.indexCount, GL_UNSIGNED_INT, nullptr);

    r.AddDrawCall();
    r.AddDrawObject();
    return true;
}

//============================================================
// Overlay（フルスクリーン系）
//============================================================
bool DispatchOverlay(Renderer& r,
                     const RenderItem& it,
                     RenderPass pass,
                     int)
{
    if (pass == RenderPass::Shadow)
        return true;

    auto* sh = it.shader.ptr;

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
bool DispatchDebug(Renderer&,
                   const RenderItem& it,
                   RenderPass pass,
                   int)
{
    if (pass == RenderPass::Shadow)
        return true;

    it.shader.ptr->SetVectorUniform("uSolColor", it.color);
    return false;
}

static bool DispatchSurface(Renderer& r,
                            const RenderItem& it,
                            RenderPass pass,
                            int)
{
    if (pass != RenderPass::World)
        return true; // 描かない

    auto* sh = it.shader.ptr;
    sh->SetActive();

    // 行列（Surface用は分解してる前提）
    sh->SetMatrixUniform("uWorld", it.world);
    sh->SetMatrixUniform("uView",  r.GetViewMatrix());
    sh->SetMatrixUniform("uProj",  r.GetProjectionMatrix());

    // パラメータ
    sh->SetBooleanUniform("uFlipX", it.surfaceFlipX);
    sh->SetBooleanUniform("uFlipY", it.surfaceFlipY);
    sh->SetFloatUniform  ("uOpacity", it.surfaceOpacity);
    sh->SetVectorUniform ("uTint", it.surfaceTint);
    sh->SetIntUniform    ("uMode", it.surfaceMode);
    sh->SetFloatUniform  ("uTime", it.time);

    // テクスチャ
    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);
        sh->SetIntUniform("uSurfaceTex", it.textureUnit);
    }

    return false; // ★通常の DrawDefaultGeometry に流す
}


RenderItem::DispatchFn GetDispatch(RenderItemType type)
{
    switch (type)
    {
        case RenderItemType::Sprite:       return &DispatchSprite;
        case RenderItemType::Mesh:         return &DispatchMesh;
        case RenderItemType::SkinnedMesh:  return &DispatchSkinnedMesh;
        case RenderItemType::Billboard:    return &DispatchBillboard;
        case RenderItemType::GPUParticle:  return &DispatchGPUParticle;
        case RenderItemType::SkyDome:      return &DispatchSkyDome;
        case RenderItemType::Overlay:      return &DispatchOverlay;
        case RenderItemType::Debug:        return &DispatchDebug;
        case RenderItemType::Surface:      return &DispatchSurface;
        default:                           return nullptr;
    }
}


} // namespace toy
