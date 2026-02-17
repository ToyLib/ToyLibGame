//======================================================================
// Render/VK/VKUBO.h
//  - Vulkan UBO definitions (std140) : SINGLE SOURCE OF TRUTH
//  - MUST match GLSL layouts exactly.
//======================================================================
#pragma once

#include "Utils/MathUtil.h" // Matrix4
#include <cstdint>
#include <cstddef>

namespace toy
{

//==============================================================
// UBO_WorldCommon  (set=1, binding=0)
//==============================================================
struct alignas(16) UBO_WorldCommon
{
    Matrix4 uViewProj;

    float uCameraPos[4];

    float uAmbientLight[4];

    float uFogMaxDist;
    float uFogMinDist;
    float _pad2[2];

    float uFogColor[4];

    Matrix4 uLightViewProj0;
    Matrix4 uLightViewProj1;

    float uCascadeSplit0;
    float uCascadeBlend;
    float uShadowBias;
    int   uUseShadow;

    int   uUseToon;
    float _pad4[3];
};
static_assert(sizeof(UBO_WorldCommon) % 16 == 0);


//==============================================================
// UBO_DirLight (set=1, binding=2)
//==============================================================
struct alignas(16) UBO_DirLight
{
    float mDirection[3];    float _p0;
    float mDiffuseColor[3]; float _p1;
    float mSpecColor[3];    float _p2;
};

//==============================================================
// UBO_PointLight / Block (set=1, binding=3)
//==============================================================
struct alignas(16) UBO_PointLight
{
    float position[3]; float intensity;
    float color[3];    float constant;

    float linear;
    float quadratic;
    float radius;
    float _p;
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
// UBO_SpriteCommon (set=1, binding=0)  ※ Sprite専用
// GLSL:
// layout(set=1,binding=0,std140,row_major) uniform SpriteCommon { mat4 uViewProj; } sc;
//==============================================================
struct alignas(16) UBO_SpriteCommon
{
    Matrix4 uViewProj;
};


//==============================================================
// Safety checks
//==============================================================
static_assert(alignof(UBO_WorldCommon) == 16, "UBO_WorldCommon must be 16-byte aligned");
static_assert(sizeof(UBO_WorldCommon) % 16 == 0, "UBO_WorldCommon size must be multiple of 16");

static_assert(alignof(UBO_DirLight) == 16, "UBO_DirLight must be 16-byte aligned");
static_assert(sizeof(UBO_DirLight) % 16 == 0, "UBO_DirLight size must be multiple of 16");

static_assert(alignof(UBO_PointLight) == 16, "UBO_PointLight must be 16-byte aligned");
static_assert(sizeof(UBO_PointLight) % 16 == 0, "UBO_PointLight size must be multiple of 16");

static_assert(alignof(UBO_PointLightBlock) == 16, "UBO_PointLightBlock must be 16-byte aligned");
static_assert(sizeof(UBO_PointLightBlock) % 16 == 0, "UBO_PointLightBlock size must be multiple of 16");

static_assert(alignof(UBO_SpriteCommon) == 16, "UBO_SpriteCommon must be 16-byte aligned");
static_assert(sizeof(UBO_SpriteCommon) % 16 == 0, "UBO_SpriteCommon size must be multiple of 16");

} // namespace toy
