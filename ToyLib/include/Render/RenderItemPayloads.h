// Render/RenderItemPayloads.h
#pragma once

#include "Utils/MathUtil.h"
#include <cstddef>
#include <cstdint>

namespace toy {

//==============================
// SpritePayload
//==============================
struct SpritePayload
{
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };
};

//==============================
// MeshPayload
//==============================
struct MeshPayload
{
    bool    toon { false };

    bool    overrideColor { false };
    Vector3 overrideColorValue { 0.0f, 0.0f, 0.0f };
};

//==============================
// SkinnedMeshPayload
//  - palette は RenderItem に残す（描画形態）
//==============================
struct SkinnedMeshPayload
{
    bool    toon { false };

    bool    overrideColor { false };
    Vector3 overrideColorValue { 0.0f, 0.0f, 0.0f };
};

//==============================
// BillboardPayload
//  - Unlit tint 拡張にも使える
//==============================
struct BillboardPayload
{
    // 互換：uUseTint==0 なら無視される想定
    bool    useTint { false };
    Vector3 tint    { 1.0f, 1.0f, 1.0f };
    float   alpha   { 1.0f };
};

//==============================
// ParticlePayload
//==============================
struct ParticlePayload
{
    Vector3 cameraRight;
    Vector3 cameraUp;

    float particleLifeMax { 1.0f };
    float particleSize    { 1.0f };
};

//==============================
// SkyDomePayload
//==============================
struct SkyDomePayload
{
    float skyTime        { 0.0f };
    float skyTimeOfDay   { 0.0f };
    int   skyWeatherType { 0 };

    Vector3 skySunDir        { Vector3::UnitY };
    Vector3 skyMoonDir       { Vector3::NegUnitY };
    Vector3 skyRawSkyColor   { Vector3::Zero };
    Vector3 skyRawCloudColor { Vector3::Zero };

    // MVP 直指定（旧互換）
    Matrix4 mvp    { Matrix4::Identity };
    bool    useMVP { false };
};

//==============================
// OverlayPayload
//==============================
struct OverlayPayload
{
    float   time { 0.0f };

    float   rainAmount { 0.0f };
    float   fogAmount  { 0.0f };
    float   snowAmount { 0.0f };

    Vector2 resolution { 0.0f, 0.0f };

    float   flareIntensity { 0.0f };
    Vector2 sunPos         { 0.0f, 0.0f };
    Vector3 flareColor     { 1.0f, 0.9f, 0.7f };
};

//==============================
// DebugPayload
//==============================
struct DebugPayload
{
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };
};

//==============================
// SurfacePayload
//==============================
struct SurfacePayload
{
    float   opacity { 1.0f };
    Vector3 tint    { 1.0f, 1.0f, 1.0f };
    bool    flipX   { false };
    bool    flipY   { false };
    int     mode    { 0 };
    float   time    { 0.0f };
    float   scanlineStrength { 0.5f };
};

} // namespace toy
