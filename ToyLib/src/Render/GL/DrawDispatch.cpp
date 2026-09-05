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
#include <iostream>

namespace toy {

//============================================================
// ボーンパレット UBO
//   binding = 0 固定（GLShader::Load() で glUniformBlockBinding(prog,"BonePalette",0) 済み）
//   シェーダ宣言: layout(std140) uniform BonePalette { mat4 matrixPalette[256]; };
//
//   ★256 = GL_MAX_UNIFORM_BLOCK_SIZE の仕様保証最小値(16384byte)に収まる上限
//     (256 * 64byte = 16384byte)。保証を超えるサイズだと環境依存でリンク失敗する
//     （AMD環境で実際に発生：320本にしたら起動不能になった）。
//     320本超の大規模リグを使いたい場合はVKバックエンド（SSBO、保証128MB）を使うこと。
//============================================================
static constexpr int    kBonePaletteMax     = 256;
static constexpr GLuint kBonePaletteBinding = 0;

static GLuint GetOrCreateBoneUBO()
{
    static GLuint s_boneUBO = 0;
    if (s_boneUBO == 0)
    {
        glGenBuffers(1, &s_boneUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, s_boneUBO);
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
    if (count > static_cast<size_t>(kBonePaletteMax))
    {
        // ボーン数がGLの上限を超えると、上限を超えた分は切り捨てられ、
        // シェーダ側のボーンIDがそれを参照すると見た目が壊れる。
        // 1回だけ警告し、スパムしない（毎フレーム呼ばれるため）。
        static bool s_warned = false;
        if (!s_warned)
        {
            std::cerr << "[GLRenderer] WARNING: skinned mesh bone count (" << count
                      << ") exceeds GL palette limit (" << kBonePaletteMax
                      << "). Bones beyond the limit are dropped and rendering may be corrupted. "
                      << "Use the VK backend (SSBO, no practical limit) for rigs this large.\n";
            s_warned = true;
        }
    }

    const size_t uploadCount = std::min(count, static_cast<size_t>(kBonePaletteMax));
    GLuint ubo = GetOrCreateBoneUBO();
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);

    // オーファニング: 同一バッファをスキンメッシュ数×カスケード数ぶん
    // 毎フレーム連続で書き換えるため、事前に glBufferData(nullptr) で
    // 古いストレージを捨てておかないと、GPU が前回の描画で読み終わるまで
    // glBufferSubData がブロックする（特に macOS の GL ドライバで顕著）。
    glBufferData(GL_UNIFORM_BUFFER,
                 kBonePaletteMax * 16 * static_cast<GLsizeiptr>(sizeof(float)),
                 nullptr,
                 GL_DYNAMIC_DRAW);

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
    // 同一パス内では同じ値のことが多いので変化時のみ送信する
    sh->SetViewProjUniformIfChanged(it.viewProj);

    // NOTE:
    // Sprite系は contract(v1) に含めてない想定なので、従来名を維持（動作維持）
    sh->SetVectorUniform("uSpriteColor", color);
    sh->SetFloatUniform ("uSpriteAlpha", alpha);

    if (it.texture.ptr)
    {
        it.texture.ptr->SetActive(it.textureUnit);
        sh->SetTextureUniform("uTexture", it.textureUnit);
        sh->SetBooleanUniform("uUseTexture", true);

        // SceneCapture(Fixed等)のRTテクスチャはGLでは上下逆に格納されるため、
        // Texture 側のフラグ（IsCaptureFlippedY）を見て自動的に補正する。
        // VK側のテクスチャは常に false（正立済み）なので補正されない。
        sh->SetIntUniform("uFlipY", it.texture.ptr->IsCaptureFlippedY() ? 1 : 0);
    }
    else
    {
        sh->SetBooleanUniform("uUseTexture", false);
        sh->SetIntUniform("uFlipY", 0);
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
    sh->SetViewProjUniformIfChanged(it.viewProj);

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
    //   VK は Capture 描画時に viewport.height を負値にして正立させて焼き込むため、
    //   テクスチャは常に正しい向き（Texture::IsCaptureFlippedY()==false）。
    //   GL はその補正を行わずに FBO へ描画するため、テクスチャ側で
    //   IsCaptureFlippedY()==true が立つ。ここでは「シーン側が要求する flipY」と
    //   「バックエンド起因の反転」を XOR することで、GL/VK 双方で同じ SetFlip() 呼び出しが
    //   同じ見え方になるようにする。
    const bool texFlippedY = (it.texture.ptr && it.texture.ptr->IsCaptureFlippedY());
    sh->SetIntUniform("uFlipX", flipX ? 1 : 0);
    sh->SetIntUniform("uFlipY", (flipY != texFlippedY) ? 1 : 0);
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
