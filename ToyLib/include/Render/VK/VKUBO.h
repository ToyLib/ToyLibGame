// Render/VK/VKUBO.h
#pragma once

#include "Utils/MathUtil.h" // Matrix4, Vector3
#include <cstdint>

namespace toy
{

// std140: vec3 は 16byte アライン扱いになるので pad が必要
struct alignas(16) Vec3Pad
{
    Vector3 v;
    float   pad { 0.0f };
};

struct alignas(16) UBO_WorldCommon
{
    Matrix4 uViewProj;

    Vec3Pad uCameraPos;
    Vec3Pad uAmbientLight;

    float uFogMaxDist { 1000.0f };
    float uFogMinDist { 500.0f };
    float _padFog0 { 0.0f };
    float _padFog1 { 0.0f };

    Vec3Pad uFogColor;

    // 互換維持（今は使わない）
    Matrix4 uLightViewProj0;
    Matrix4 uLightViewProj1;

    float uCascadeSplit0 { 0.0f };
    float uCascadeBlend  { 0.0f };
    float uShadowBias    { 0.0f };
    int32_t uUseShadow   { 0 };
    int32_t uUseToon     { 0 };
    float _pad4a { 0.0f };
    float _pad4b { 0.0f };
};



struct alignas(16) UBO_MaterialParams
{
    float uDiffuseColor[3];
    int   uUseTexture;

    float uUniformColor[3];
    int   uOverrideColor;

    float uSpecPower;
    float _pad0;
    float _pad1;
    float _pad2;
};
static_assert(sizeof(UBO_MaterialParams) == 48);

struct alignas(16) UBO_DirLight
{
    Vec3Pad mDirection;
    Vec3Pad mDiffuseColor;
    Vec3Pad mSpecColor;
};

struct alignas(16) UBO_PointLight
{
    // vec3 position; float intensity;
    Vec3Pad position;
    float   intensity { 1.0f };
    float   _pad0a { 0.0f };
    float   _pad0b { 0.0f };
    float   _pad0c { 0.0f };

    // vec3 color; float constant;
    Vec3Pad color;
    float   constant { 1.0f };
    float   linear { 0.0f };
    float   quadratic { 0.0f };
    float   radius { 0.0f };
    float   _p { 0.0f };
};

struct alignas(16) UBO_PointLightBlock
{
    int32_t uNumPointLights { 0 };
    int32_t _pA { 0 };
    int32_t _pB { 0 };
    int32_t _pC { 0 };

    UBO_PointLight uPointLights[8];
};

} // namespace toy
