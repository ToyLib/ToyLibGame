// Render/DrawDispatch.cpp
#include "Render/RenderItem.h"

#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"
#include "Render/IRenderer.h"
#include "Render/GL/GLShader.h"
#include "Render/GL/UniformNamesGL.h"

#include "glad/glad.h"
#include <algorithm>

namespace toy {

//============================================================
// ボーンパレット UBO
//   binding = 0 固定（GLShader::Load() で glUniformBlockBinding(prog,"BonePalette",0) 済み）
//   シェーダ宣言: layout(std140) uniform BonePalette { mat4 matrixPalette[320]; };
//============================================================
static constexpr int    kBonePaletteMax     = 320;
static constexpr GLuint kBonePaletteBinding = 0;

static GLuint GetOrCreateBoneUBO()
{
    static GLuint s_boneUBO = 0;
    if (s_boneUBO == 0)
    {
        glGenBuffers(1, &s_boneUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, s_boneUBO);
        // std140 で mat4 は 64 バイト → 320 × 64 = 20480 バイト（UBO 保証 64KB 以内）
        glBufferData(GL_UNIFORM_BUFFER,
                     kBonePaletteMax * 16 * static_cast<GLsizeiptr>(sizeof(float)),
                     nullptr,
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    return s_boneUBO;
}

static void UploadBonePalette(const Matrix4* palette, size_t count)
{
    const size_t uploadCount = std::min(count, static_cast<size_t>(kBonePaletteMax));
    GLuint ubo = GetOrCreateBoneUBO();
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(uploadCount * 16 * sizeof(float)),
                    palette[0].GetAsFloatPtr());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, kBonePaletteBinding, ubo);
}

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

    // 2D/UI 用 VP 行列（SceneUBO は 3D 透視行列なので per-draw で上書き）
    sh->SetMatrixUniform("uViewProj", it.viewProj);

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
    // Payload（toon / overrideColor）
    //  - Object::World は SetCommonUniforms() で設定済み
    //  - viewProj は SceneUBO（binding=1）で供給される
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

    // ライティング・シャドウパラメータは SceneUBO（binding=1）で供給される。
    // シャドウマップテクスチャのみ個別にバインドする（opaque 型は UBO に入れられない）。
    if (auto sm0 = r.GetShadowMapTexture(0)) sm0->SetActive(6);
    if (auto sm1 = r.GetShadowMapTexture(1)) sm1->SetActive(7);

    sh->SetTextureUniform(toy::glsl::Scene::ShadowMap0, 6);
    sh->SetTextureUniform(toy::glsl::Scene::ShadowMap1, 7);

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
            UploadBonePalette(it.matrixPalette, it.paletteCount);
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

    // Object::World は SetCommonUniforms() で設定済み。
    // ライティング・シャドウパラメータは SceneUBO（binding=1）で供給される。
    // シャドウマップは overrideColor に関わらず常にバインドする
    // （スキップするとスロット 6/7 に前フレームの残骸が残り、影が壊れる）
    if (auto sm0 = r.GetShadowMapTexture(0))
    {
        sm0->SetActive(6);
    }
    if (auto sm1 = r.GetShadowMapTexture(1))
    {
        sm1->SetActive(7);
    }

    sh->SetTextureUniform(Scene::ShadowMap0, 6);
    sh->SetTextureUniform(Scene::ShadowMap1, 7);

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
        UploadBonePalette(it.matrixPalette, it.paletteCount);
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

    //========================================================
    // Contract(v2): Object::World は SetCommonUniforms で設定済み
    // viewProj は SceneUBO の 3D 透視行列とは別に per-draw で設定
    //========================================================
    sh->SetMatrixUniform(toy::glsl::Object::World, it.world);
    sh->SetMatrixUniform("uViewProj",              it.viewProj);

    // Payload
    Vector3 tint  = Vector3(1.0f, 1.0f, 1.0f);
    float   alpha = 1.0f;

    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
        const UnlitQuadPayload& up = r.GetUnlitQuadPayload(it.payloadIndex);
        tint  = up.tint;
        alpha = up.alpha;
    }

    // 旧互換
    sh->SetVectorUniform("uSpriteColor", tint);
    sh->SetFloatUniform ("uSpriteAlpha", alpha);

    // Unlit material
    sh->SetVectorUniform(toy::glsl::Material::BaseColor, Vector3(1.0f, 1.0f, 1.0f));

    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);

        sh->SetTextureUniform(toy::glsl::Material::BaseMap, it.textureUnit);
        sh->SetBooleanUniform(toy::glsl::Material::UseTexture, true);

        // 旧互換
        sh->SetTextureUniform("uTexture", it.textureUnit);
        sh->SetBooleanUniform("uUseTexture", true);
    }
    else
    {
        sh->SetBooleanUniform(toy::glsl::Material::UseTexture, false);
        sh->SetBooleanUniform("uUseTexture", false);
    }

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
    if (!sh || !it.geometry.ptr || it.indexCount == 0)
    {
        return true;
    }

    sh->SetActive();

    SkyDomePayload sky{};
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
    if (!sh || !it.geometry.ptr || it.indexCount == 0)
    {
        return true;
    }

    sh->SetActive();

    OverlayPayload op{};
    if (it.payloadIndex != RenderItem::kInvalidPayload)
    {
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

    // Contract(v2): RenderSurface.vert は uObject.world と SceneUBO の
    // uScene.viewProj を参照する（uWorld/uView/uProj は廃止済みの名前で、
    // 存在しないためこの呼び出しは無効化されていた＝サーフェスが描画されない
    // 原因になっていた）。
    sh->SetMatrixUniform(toy::glsl::Object::World, it.world);

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

    // uFlipX / uFlipY: bool uniform は GL 4.1 で SetBooleanUniform が誤動作する場合があるため
    // int で送る（RenderSurface.frag 側も uniform int に統一）。
    // Mirror(mode==2) は反射カメラの X 軸反転が RTT に含まれているため、
    // uFlipX=1 で UV を再反転して正しいミラー像を得る（flipX=true がデフォルト）。
    //
    // uFlipY について:
    //   VK は Capture 描画時に viewport.height を負値にして正立させているため
    //   flipY=false のままで正しく表示される（FieldScene 等の SetFlip(true,false)）。
    //   GL 側はその補正を行わずに FBO へ描画するため、テクスチャの v=0 が
    //   キャプチャ画面の下端（地面側）を指してしまい、そのまま false で使うと
    //   上下が反転して見える。GL では常に反転して補正する。
    sh->SetIntUniform("uFlipX", flipX ? 1 : 0);
    sh->SetIntUniform("uFlipY", flipY ? 0 : 1);
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
