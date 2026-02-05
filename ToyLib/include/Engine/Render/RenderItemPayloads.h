// Engine/Render/RenderItemPayloads.h
#pragma once

#include "Utils/MathUtil.h"
#include <cstddef>
#include <cstdint>

namespace toy {

//--------------------------------------------------------------
// Sprite
//--------------------------------------------------------------
struct SpritePayload
{
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };
};

//--------------------------------------------------------------
// Mesh
// いま RenderItem にある “Contour/OverrideColor” は Mesh/Skinned だけっぽいのでここへ
//--------------------------------------------------------------
struct MeshPayload
{
    bool    overrideColor      { false };
    Vector3 overrideColorValue { 0.0f, 0.0f, 0.0f };
};

//--------------------------------------------------------------
// SkinnedMesh
//--------------------------------------------------------------
struct SkinnedMeshPayload
{
    // Material override
    bool    overrideColor      { false };
    Vector3 overrideColorValue { 0.0f, 0.0f, 0.0f };

    // Skinning
    const Matrix4* matrixPalette { nullptr };
    size_t         paletteCount  { 0 };
};

//--------------------------------------------------------------
// Billboard
//--------------------------------------------------------------
struct BillboardPayload
{
    // 現状 Billboard 専用が “実は無い” なら空でもOK（とりあえず枠だけ）
    // 将来: billboard params etc.
};

//--------------------------------------------------------------
// Particle (GPU particle)
//--------------------------------------------------------------
struct ParticlePayload
{
    unsigned int gpuVAO        { 0 };
    int          instanceCount { 0 };

    Vector3 cameraRight     { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp        { 0.0f, 1.0f, 0.0f };
    float   particleLifeMax { 1.0f };
    float   particleSize    { 1.0f };
};

//--------------------------------------------------------------
// SkyDome
//--------------------------------------------------------------
struct SkyDomePayload
{
    float skyTime        { 0.0f };
    float skyTimeOfDay   { 0.0f };
    int   skyWeatherType { 0 };

    Vector3 skySunDir        { Vector3::UnitY };
    Vector3 skyMoonDir       { Vector3::NegUnitY };
    Vector3 skyRawSkyColor   { Vector3::Zero };
    Vector3 skyRawCloudColor { Vector3::Zero };

    Vector3 skyFogColor   { Vector3::Zero };
    float   skyFogDensity { 0.0f };

    // MVP直指定（SkyDome互換）
    Matrix4 mvp    { Matrix4::Identity };
    bool    useMVP { false };
};

//--------------------------------------------------------------
// Overlay
//--------------------------------------------------------------
struct OverlayPayload
{
    float   overlayTime { 0.0f };

    float   overlayRainAmount { 0.0f };
    float   overlayFogAmount  { 0.0f };
    float   overlaySnowAmount { 0.0f };

    Vector2 overlayResolution { Vector2::Zero };

    // flare
    float   overlayFlareIntensity { 0.0f };
    Vector2 overlaySunPos         { Vector2::Zero };
    Vector3 overlayFlareColor     { 1.0f, 0.9f, 0.7f };
};

//--------------------------------------------------------------
// Debug
//--------------------------------------------------------------
struct DebugPayload
{
    Vector3 color { 1.0f, 1.0f, 1.0f }; // uSolColor 用
};

//--------------------------------------------------------------
// Surface
//--------------------------------------------------------------
struct SurfacePayload
{
    float   surfaceOpacity    { 1.0f };
    Vector3 surfaceTint       { 1.0f, 1.0f, 1.0f };
    bool    surfaceFlipX      { false };
    bool    surfaceFlipY      { false };
    int     surfaceMode       { 0 };
    float   time              { 0.0f };
    float   scanlineStrength  { 1.0f };
};

} // namespace toy
