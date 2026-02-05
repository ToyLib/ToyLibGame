// Engine/Render/RenderItemPayloads.h
#pragma once

#include "Utils/MathUtil.h"
#include <cstdint>
#include <cstddef>

namespace toy
{

//==============================================================
// SpritePayload
//==============================================================
struct SpritePayload
{
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };
};

//==============================================================
// MeshPayload
//  - 今は Mesh 固有というより「Material側に寄せたい」ものが多いので最小。
//==============================================================
struct MeshPayload
{
    bool    overrideColor { false };
    Vector3 overrideColorValue { 0.0f, 0.0f, 0.0f };

    bool    toon { false };
};

//==============================================================
// SkinnedMeshPayload
//==============================================================
struct SkinnedMeshPayload
{
    const Matrix4* matrixPalette { nullptr };
    size_t         paletteCount  { 0 };

    bool    overrideColor { false };
    Vector3 overrideColorValue { 0.0f, 0.0f, 0.0f };

    bool toon { false };
};

//==============================================================
// BillboardPayload
//  - 今は特別なuniformが少ないので空でもOK。将来拡張用。
//==============================================================

struct BillboardPayload
{
    // 今の DispatchBillboard は texture は RenderItem 側の handle を見てるので、
    // payload に持たせなくてもOK。
    // 将来的に色/アルファや特殊パラメータが増えるならここに追加。
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };
};

//==============================================================
// ParticlePayload (GPU instancing)
//==============================================================
struct ParticlePayload
{
    Vector3 cameraRight;
    Vector3 cameraUp;

    float particleLifeMax { 1.0f };
    float particleSize    { 1.0f };
};

//==============================================================
// SkyDomePayload
//==============================================================
struct SkyDomePayload
{
    float skyTime        { 0.0f }; // 雲アニメ等（0..1）
    float skyTimeOfDay   { 0.0f }; // 1日（0..1）
    int   skyWeatherType { 0 };

    Vector3 skySunDir        { Vector3::UnitY };
    Vector3 skyMoonDir       { Vector3::NegUnitY };
    Vector3 skyRawSkyColor   { Vector3::Zero };
    Vector3 skyRawCloudColor { Vector3::Zero };

    Vector3 skyFogColor   { Vector3::Zero };
    float   skyFogDensity { 0.0f };

    // MVP 直指定（旧 WeatherDome 互換）
    Matrix4 mvp    { Matrix4::Identity };
    bool    useMVP { false };
};


//==============================================================
// OverlayPayload
//==============================================================
struct OverlayPayload
{
    float   time { 0.0f };

    float   rainAmount { 0.0f };
    float   fogAmount  { 0.0f };
    float   snowAmount { 0.0f };

    Vector2 resolution { 0.0f, 0.0f };

    // flare
    float   flareIntensity { 0.0f };
    Vector2 sunPos         { 0.0f, 0.0f };
    Vector3 flareColor     { 1.0f, 0.9f, 0.7f };
};

//==============================================================
// DebugPayload
//==============================================================
struct DebugPayload
{
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };
};

//==============================================================
// SurfacePayload
//==============================================================
struct SurfacePayload
{
    float   opacity { 1.0f };
    Vector3 tint    { 1.0f, 1.0f, 1.0f };
    bool    flipX   { false };
    bool    flipY   { false };
    int     mode    { 0 };
    float   time    { 0.0f };
};

} // namespace toy
