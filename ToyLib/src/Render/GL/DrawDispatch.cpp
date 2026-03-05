// Render/DrawDispatch.cpp
#include "Render/RenderItem.h"

#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"
#include "Render/LightingManager.h"
#include "Render/IRenderer.h"
#include "Render/GL/GLShader.h"
#include "Render/GL/UniformNamesGL.h"

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

    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const SpritePayload& sp = r.GetSpritePayload(it.payloadIndex);
        color = sp.color;
        alpha = sp.alpha;
    }

    // NOTE:
    // Sprite系は contract(v1) に含めてない想定なので、従来名を維持（動作維持）
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


    //========================================================
    // Contract(v1)
    //   - uObject.world
    //   - uScene.viewProj
    //========================================================
    sh->SetMatrixUniform(toy::glsl::Object::World,   it.world);
    sh->SetMatrixUniform(toy::glsl::Scene::ViewProj, it.viewProj);

    //========================================================
    // Payload（toon / overrideColor）
    //========================================================
    bool    toon          = false;
    bool    overrideColor = false;
    Vector3 overrideValue = Vector3(0.0f, 0.0f, 0.0f);

    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const MeshPayload& mp = r.GetMeshPayload(it.payloadIndex);
        toon          = mp.toon;
        overrideColor = mp.overrideColor;
        overrideValue = mp.overrideColorValue;
    }

    // ライティング
    if (r.GetLightingManager())
    {
        const Matrix4 view = r.GetViewMatrix();
        r.GetLightingManager()->ApplyToShader(sh, view);
    }

    // Shadow maps bind
    if (auto sm0 = r.GetShadowMapTexture(0)) sm0->SetActive(6);
    if (auto sm1 = r.GetShadowMapTexture(1)) sm1->SetActive(7);

    sh->SetTextureUniform(toy::glsl::Scene::ShadowMap0, 6);
    sh->SetTextureUniform(toy::glsl::Scene::ShadowMap1, 7);

    sh->SetMatrixUniform(toy::glsl::Scene::LightVP0, r.GetLightSpaceMatrix(0));
    sh->SetMatrixUniform(toy::glsl::Scene::LightVP1, r.GetLightSpaceMatrix(1));

    sh->SetFloatUniform(toy::glsl::Scene::CascadeSplit0, r.GetCascadeSplit0());
    sh->SetFloatUniform(toy::glsl::Scene::CascadeBlend,  r.GetCascadeBlend());
    sh->SetFloatUniform(toy::glsl::Scene::ShadowBias,    r.GetShadowBias());

    // Toon
    sh->SetBooleanUniform(toy::glsl::Material::Toon, toon);

    // Material（overrideColor 対応）
    if (it.material.ptr)
    {
        if (overrideColor)
        {
            it.material.ptr->SetOverrideColor(true, overrideValue);
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
        return true;
    }

    using namespace toy::glsl;

    // Payload（toon / override）
    const SkinnedMeshPayload* p = nullptr;
    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        p = &r.GetSkinnedMeshPayload(it.payloadIndex);
    }

    const bool    toon = (p ? p->toon : false);
    const bool    overrideColor = (p ? p->overrideColor : false);
    const Vector3 overrideColorValue =
        (p ? p->overrideColorValue : Vector3(0.0f, 0.0f, 0.0f));

    //============================================================
    // Shadow pass
    //============================================================
    if (pass == RenderPass::Shadow)
    {
        sh->SetMatrixUniform(Object::World, it.world);

        if (it.matrixPalette && it.paletteCount > 0)
        {
            sh->SetMatrixUniforms(Skinned::MatrixPalette0,
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

    sh->SetMatrixUniform(Object::World, it.world);
    sh->SetMatrixUniform(Scene::ViewProj, it.viewProj);

    if (r.GetLightingManager())
    {
        const Matrix4 view = r.GetViewMatrix();
        r.GetLightingManager()->ApplyToShader(sh, view);
    }

    if (!overrideColor)
    {
        if (auto sm0 = r.GetShadowMapTexture(0)) sm0->SetActive(6);
        if (auto sm1 = r.GetShadowMapTexture(1)) sm1->SetActive(7);

        sh->SetTextureUniform(Scene::ShadowMap0, 6);
        sh->SetTextureUniform(Scene::ShadowMap1, 7);

        sh->SetMatrixUniform(Scene::LightVP0, r.GetLightSpaceMatrix(0));
        sh->SetMatrixUniform(Scene::LightVP1, r.GetLightSpaceMatrix(1));

        sh->SetFloatUniform(Scene::CascadeSplit0, r.GetCascadeSplit0());
        sh->SetFloatUniform(Scene::CascadeBlend,  r.GetCascadeBlend());
        sh->SetFloatUniform(Scene::ShadowBias,    r.GetShadowBias());
    }

    sh->SetBooleanUniform(toy::glsl::Material::Toon, toon);

    if (it.material.ptr)
    {
        if (overrideColor)
        {
            it.material.ptr->SetOverrideColor(true, overrideColorValue);
        }

        it.material.ptr->BindToShader(sh, 0);

        if (overrideColor)
        {
            it.material.ptr->SetOverrideColor(false, Vector3(0.0f, 0.0f, 0.0f));
        }
    }

    if (it.matrixPalette && it.paletteCount > 0)
    {
        sh->SetMatrixUniforms(Skinned::MatrixPalette0,
                              it.matrixPalette,
                              static_cast<unsigned int>(it.paletteCount));
    }

    return false;
}

//============================================================
// UnlitQuad
//============================================================
static bool DispatchUnlitQuad(IRenderer& r,
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

    // Payload
    Vector3 tint  = Vector3(1.0f, 1.0f, 1.0f);
    float   alpha = 1.0f;

    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const UnlitQuadPayload& bp = r.GetUnlitQuadPayload(it.payloadIndex);
        tint  = bp.tint;   // ★ここ
        alpha = bp.alpha;
    }

    // NOTE: Billboard系は contract(v1) に含めてない想定なので、従来名も維持
    // （古い billboard shader が uSpriteColor/uSpriteAlpha を見ている想定）
    sh->SetVectorUniform("uSpriteColor", tint);
    sh->SetFloatUniform ("uSpriteAlpha", alpha);

    // ------------------------------------------------------------
    // Unlit 対応：
    // Unlit.frag は uMaterial.* を参照するので、ここでセットする
    // ------------------------------------------------------------
    sh->SetVectorUniform(toy::glsl::Material::BaseColor, Vector3(1.0f, 1.0f, 1.0f));

    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);

        // 新契約（Unlit / MeshPhong 互換）
        sh->SetTextureUniform(toy::glsl::Material::BaseMap, it.textureUnit);
        sh->SetBooleanUniform(toy::glsl::Material::UseTexture, true);

        // 旧契約（古い billboard shader 用）
        sh->SetTextureUniform("uTexture", it.textureUnit);
        sh->SetBooleanUniform("uUseTexture", true);
    }
    else
    {
        // 新契約
        sh->SetBooleanUniform(toy::glsl::Material::UseTexture, false);
    }

    // ------------------------------------------------------------
    // ★Unlit tint 拡張（ここを入れると FootSprite/ShadowSprite も統一できる）
    //   - TextBillboard は uUseTint を触らない運用でもいいけど、
    //     触っても tint=1, alpha=1 なら見た目は同じ。
    // ------------------------------------------------------------
    sh->SetIntUniform   ("uUseTint", 1);
    sh->SetVectorUniform("uTint",    tint);
    sh->SetFloatUniform ("uAlpha",   alpha);

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

    // NOTE: Particle固有uniformは contract(v1) 外の想定なので従来名維持
    sh->SetVectorUniform("uCameraRight", camRight);
    sh->SetVectorUniform("uCameraUp",    camUp);
    sh->SetFloatUniform ("uLifeMax",     lifeMax);
    sh->SetFloatUniform ("uSize",        size);

    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);
        sh->SetTextureUniform("uTexture", it.textureUnit);
    }

    return false;
}

//============================================================
// SkyDome
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

    SkyDomePayload sky {};
    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        sky = r.GetSkyDomePayload(it.payloadIndex);
    }

    // NOTE: SkyDomeは contract(v1) 外なので従来名維持
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

    it.geometry.ptr->SetActive();
    glDrawElements(GL_TRIANGLES, it.indexCount, GL_UNSIGNED_INT, nullptr);

    r.AddDrawCall();
    return true;
}

//============================================================
// Overlay
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

    OverlayPayload op {};
    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        op = r.GetOverlayPayload(it.payloadIndex);
    }

    // NOTE: Overlayは contract(v1) 外なので従来名維持
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

    // NOTE: Debugは contract(v1) 外なので従来名を維持
    sh->SetVectorUniform("uSolColor", color);

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

    // NOTE: Surface は別契約（uWorld/uView/uProj）を維持（動作維持）
    sh->SetMatrixUniform("uWorld", it.world);
    sh->SetMatrixUniform("uView",  r.GetViewMatrix());
    sh->SetMatrixUniform("uProj",  r.GetProjectionMatrix());

    bool    flipX   = false;
    bool    flipY   = false;
    float   opacity = 1.0f;
    Vector3 tint    = Vector3(1.0f, 1.0f, 1.0f);
    int     mode    = 0;
    float   time    = 0.0f;
    float   scanlineStrength = 1.0f;

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

    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);
        sh->SetIntUniform("uSurfaceTex", it.textureUnit);
    }

    return false;
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
        case RenderItemType::UnlitQuad:   return &DispatchUnlitQuad;
        case RenderItemType::Particle:    return &DispatchParticle;
        case RenderItemType::SkyDome:     return &DispatchSkyDome;
        case RenderItemType::Overlay:     return &DispatchOverlay;
        case RenderItemType::Debug:       return &DispatchDebug;
        case RenderItemType::Surface:     return &DispatchSurface;
        default:                          return nullptr;
    }
}

} // namespace toy
