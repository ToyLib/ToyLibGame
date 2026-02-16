//======================================================================
// Render/VK/VKUBO.h
//  - Vulkan UBO definitions (std140) : SINGLE SOURCE OF TRUTH
//  - MUST match GLSL layouts exactly.
//======================================================================
#pragma once

#include "Utils/MathUtil.h" // Matrix4
#include <cstddef>          // size_t, offsetof
#include <cstdint>

namespace toy
{

//==============================================================
// UBO_WorldCommon  (set=1, binding=0)
// GLSL:
// layout(set=1,binding=0,std140) uniform WorldCommon { ... } sc;
//==============================================================
struct alignas(16) UBO_WorldCommon
{
    // mat4 uViewProj;
    Matrix4 uViewProj;

    // vec3 uCameraPos; float _pad0;
    alignas(16) float uCameraPos[4];

    // vec3 uAmbientLight; float _pad1;
    alignas(16) float uAmbientLight[4];

    // float uFogMaxDist;
    // float uFogMinDist;
    // vec2  _pad2;
    float uFogMaxDist;
    float uFogMinDist;
    float _pad2[2];

    // vec3 uFogColor; float _pad3;
    alignas(16) float uFogColor[4];

    // mat4 uLightViewProj0;
    // mat4 uLightViewProj1;
    Matrix4 uLightViewProj0;
    Matrix4 uLightViewProj1;

    // float uCascadeSplit0;
    // float uCascadeBlend;
    // float uShadowBias;
    // int   uUseShadow;
    float uCascadeSplit0;
    float uCascadeBlend;
    float uShadowBias;
    int   uUseShadow;

    // int   uUseToon;
    // float _pad4;
    // (pad to 16 bytes)
    int   uUseToon;
    float _pad4[3];
};

//==============================================================
// UBO_MaterialParams (set=1, binding=1)
// GLSL:
// layout(set=1,binding=1,std140) uniform MaterialParams
// {
//     vec3  uDiffuseColor; int uUseTexture;
//     vec3  uUniformColor; int uOverrideColor;
//     float uSpecPower;
//     float _padM0; float _padM1; float _padM2;
// } mp;
//==============================================================
struct alignas(16) UBO_MaterialParams
{
    float uDiffuseColor[3];
    int   uUseTexture;

    float uUniformColor[3];
    int   uOverrideColor;

    float uSpecPower;
    float _padM0;
    float _padM1;
    float _padM2;
};

//==============================================================
// UBO_DirLight (set=1, binding=2)
// GLSL:
// struct DirectionalLight
// {
//     vec3 mDirection;    float _p0;
//     vec3 mDiffuseColor; float _p1;
//     vec3 mSpecColor;    float _p2;
// };
// layout(set=1,binding=2,std140) uniform DirLightBlock { DirectionalLight uDirLight; } dl;
//==============================================================
struct alignas(16) UBO_DirLight
{
    float mDirection[3];    float _p0;
    float mDiffuseColor[3]; float _p1;
    float mSpecColor[3];    float _p2;
};

//==============================================================
// UBO_PointLight / UBO_PointLightBlock (set=1, binding=3)
// GLSL:
// struct PointLight
// {
//     vec3 position; float intensity;
//     vec3 color;    float constant;
//     float linear;
//     float quadratic;
//     float radius;
//     float _p;
// };
// layout(set=1,binding=3,std140) uniform PointLightBlock
// {
//     int uNumPointLights;
//     int _pA; int _pB; int _pC;
//     PointLight uPointLights[8];
// } pl;
//==============================================================
struct alignas(16) UBO_PointLight
{
    float position[3]; float intensity;
    float color[3];    float constant;

    float linear;      // attenuation linear
    float quadratic;   // attenuation quadratic
    float radius;      // cutoff radius
    float _p;          // pad
};

struct alignas(16) UBO_PointLightBlock
{
    int uNumPointLights;
    int _pA;
    int _pB;
    int _pC;

    UBO_PointLight uPointLights[8];
};

//==============================================================
// Safety checks (std140 expectations)
//==============================================================
static_assert(alignof(UBO_WorldCommon) == 16, "UBO_WorldCommon must be 16-byte aligned");
static_assert(sizeof(UBO_WorldCommon) % 16 == 0, "UBO_WorldCommon size must be multiple of 16");

static_assert(alignof(UBO_MaterialParams) == 16, "UBO_MaterialParams must be 16-byte aligned");
static_assert(sizeof(UBO_MaterialParams) == 48, "UBO_MaterialParams must be 48 bytes (std140)");

static_assert(alignof(UBO_DirLight) == 16, "UBO_DirLight must be 16-byte aligned");
static_assert(sizeof(UBO_DirLight) == 48, "UBO_DirLight must be 48 bytes (std140)");

static_assert(alignof(UBO_PointLight) == 16, "UBO_PointLight must be 16-byte aligned");
static_assert(sizeof(UBO_PointLight) == 48, "UBO_PointLight must be 48 bytes (std140)");

static_assert(alignof(UBO_PointLightBlock) == 16, "UBO_PointLightBlock must be 16-byte aligned");
static_assert(sizeof(UBO_PointLightBlock) % 16 == 0, "UBO_PointLightBlock size must be multiple of 16");

} // namespace toy
