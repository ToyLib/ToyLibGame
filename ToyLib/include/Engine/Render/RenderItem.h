// Engine/Render/RenderItem.h
#pragma once

#include "Utils/MathUtil.h"
#include "Engine/Render/RenderEnums.h"
#include "Engine/Render/RenderHandles.h"
#include "Engine/Render/VisualLayer.h"

#include <cstddef>

namespace toy {

class Renderer;

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
    Overlay,
    Debug,
    Surface
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
    
    using DispatchFn = bool(*)(Renderer& r,
                                  const RenderItem& it,
                                  RenderPass pass,
                                  int cascadeIndex);

    DispatchFn dispatch = nullptr;

    RenderItemType type   = RenderItemType::Sprite;

    //==========================================================
    // Geometry
    //==========================================================
    PrimitiveTopology topology    = PrimitiveTopology::Triangles;
    GeometryHandle    geometry    {};
    int               indexCount  = 0;   // glDrawElements 用
    int               vertexCount = 0;   // glDrawArrays 用

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
    // Contour
    //==========================================================
    bool overrideColor  = false;
    Vector3 overrideColorValue { Vector3(0.0f, 0.0f, 0.0f) };
    
    //==========================================================
    // Transforms
    //==========================================================
    Matrix4 world    { Matrix4::Identity };
    Matrix4 viewProj { Matrix4::Identity };

    // Shadow 等で使いたい場合の予備
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
    size_t         paletteCount  = 0;

    //==========================================================
    // GPUParticle-only
    //==========================================================
    unsigned int gpuVAO        = 0;
    int          instanceCount = 0;

    Vector3 cameraRight     { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp        { 0.0f, 1.0f, 0.0f };
    float   particleLifeMax = 1.0f;
    float   particleSize    = 1.0f;

    //==========================================================
    // SkyDome / WeatherDome params
    //==========================================================
    float skyTime        = 0.0f;   // 雲アニメ等（0..1）
    float skyTimeOfDay   = 0.0f;   // 1日（0..1）
    int   skyWeatherType = 0;

    Vector3 skySunDir        = Vector3::UnitY;
    Vector3 skyMoonDir       = Vector3::NegUnitY;
    Vector3 skyRawSkyColor   = Vector3::Zero;
    Vector3 skyRawCloudColor = Vector3::Zero;

    Vector3 skyFogColor   = Vector3::Zero;
    float   skyFogDensity = 0.0f;

    // MVP 直指定（SkyDome 互換）
    Matrix4 mvp    { Matrix4::Identity };
    bool    useMVP = false;

    //==========================================================
    // Overlay / WeatherOverlay params
    //==========================================================
    float   overlayTime = 0.0f;

    float   overlayRainAmount = 0.0f;
    float   overlayFogAmount  = 0.0f;
    float   overlaySnowAmount = 0.0f;

    Vector2 overlayResolution = Vector2::Zero;

    // flare
    float   overlayFlareIntensity = 0.0f;
    Vector2 overlaySunPos         = Vector2::Zero;
    Vector3 overlayFlareColor     = Vector3(1.0f, 0.9f, 0.7f);
    
    //==========================================================
    // Surface
    //==========================================================
    float   surfaceOpacity = 1.0f;
    Vector3 surfaceTint    = Vector3(1,1,1);
    bool    surfaceFlipX   = false;
    bool    surfaceFlipY   = false;
    int     surfaceMode    = 0;     // enum 化してもOK
    float   time           = 0.0f;
    
};


RenderItem::DispatchFn GetDispatch(RenderItemType type);

} // namespace toy
