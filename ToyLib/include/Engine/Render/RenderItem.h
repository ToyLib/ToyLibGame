// Engine/Render/RenderItem.h
#pragma once

#include "Utils/MathUtil.h"
#include "Engine/Render/RenderEnums.h"
#include "Engine/Render/RenderHandles.h"
#include "Engine/Render/VisualLayer.h"

#include <cstddef>

namespace toy {

//==============================================================
// RenderItemType
//==============================================================
enum class RenderItemType
{
    Sprite,
    Mesh,
    SkinnedMesh,
    Billboard,
    GPUParticle,
    SkyDome,
    Debug
};

//==============================================================
// RenderItem
//  - Renderer が「描画1単位」として処理するデータ
//  - VisualComponent::GatherRenderItems() で積まれる
//==============================================================
struct RenderItem
{
    //==========================================================
    // Sort key / pass
    //==========================================================
    RenderPass  pass      = RenderPass::World;
    VisualLayer layer     = VisualLayer::Object3D;
    int         drawOrder = 0;

    RenderItemType type   = RenderItemType::Sprite;

    //==========================================================
    // Geometry
    //==========================================================
    PrimitiveTopology topology   = PrimitiveTopology::Triangles;
    GeometryHandle    geometry   {};
    int               indexCount = 0;   // glDrawElements 用（0なら DrawArrays）
    int               vertexCount = 0;  // glDrawArrays 用

    //==========================================================
    // Render state
    //==========================================================
    BlendMode  blend      = BlendMode::Opaque;
    bool       depthTest  = true;
    bool       depthWrite = true;
    bool       toon       = false;

    CullMode   cull       = CullMode::Back;
    FrontFace  frontFace  = FrontFace::CCW;

    //==========================================================
    // Shader / material / texture
    //==========================================================
    ShaderHandle   shader   {};
    MaterialHandle material {};
    TextureHandle  texture  {};
    int            textureUnit = 0;

    //==========================================================
    // Transforms
    //==========================================================
    Matrix4 world    { Matrix4::Identity };
    Matrix4 viewProj { Matrix4::Identity };

    // Shadow 等で使いたい場合の予備（必要になったら用途確定）
    Matrix4 lightVP  { Matrix4::Identity };

    //==========================================================
    // Sprite-only uniforms
    //==========================================================
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };

    //==========================================================
    // SkinnedMesh-only
    //==========================================================
    const Matrix4* matrixPalette = nullptr;
    size_t         paletteCount  = 0;   // <= MAX_SKELETON_BONES

    //==========================================================
    // GPUParticle-only (GL backend)
    //==========================================================
    unsigned int gpuVAO        = 0;     // instanced 用 VAO
    int          instanceCount = 0;     // インスタンス数

    // uniforms
    Vector3 cameraRight     { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp        { 0.0f, 1.0f, 0.0f };
    float   particleLifeMax = 1.0f;
    float   particleSize    = 1.0f;

    //==========================================================
    // SkyDome / WeatherDome params
    //==========================================================
    // 雲アニメ等（0..1 をループで渡す想定）
    float skyTime      = 0.0f;

    // 1日（0..1）
    float skyTimeOfDay = 0.0f;

    // WeatherType を int で（GLSL 側は int uniform）
    int   skyWeatherType = 0;

    Vector3 skySunDir  = Vector3::UnitY;
    Vector3 skyMoonDir = Vector3::NegUnitY;

    Vector3 skyRawSkyColor   = Vector3::Zero;
    Vector3 skyRawCloudColor = Vector3::Zero;

    // ★追加：SkyDome シェーダ側でもフォグを扱うなら必要
    Vector3 skyFogColor   = Vector3::Zero;
    float   skyFogDensity = 0.0f;
    
    // RenderItem.h に追加（例）
    Matrix4 mvp { Matrix4::Identity };
    bool    useMVP = false;
};

} // namespace toy
