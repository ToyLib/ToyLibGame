#pragma once

//======================================================================
//  GLShaderTypes.h
//  ・GL シェーダーと共有する CPU 側データ構造
//  ・std140 レイアウト厳守（vec3 は vec4 に拡張、行列は row_major）
//
//  SceneUBO バインディング : kSceneUBOBinding (= 1)
//======================================================================

#include <cstddef>
#include <cstdint>

namespace toy
{

//----------------------------------------------------------------------
//  GLSceneUBO
//  ・GLSL の SceneUBO ブロックと完全に一致させること
//  ・std140: vec4/mat4 は 16B アライン、float は 4B、int は 4B
//  ・row_major: ToyLib の v*M 規約に合わせた行優先行列
//
//  オフセット一覧
//    [  0] viewProj            mat4  64B
//    [ 64] cameraAndSun        vec4  16B  xyz=cameraPos, w=sunIntensity
//    [ 80] ambientLight        vec4  16B  xyz=ambient
//    [ 96] dirDirection        vec4  16B  xyz=dirLight.direction
//    [112] dirDiffuse          vec4  16B  xyz=dirLight.diffuse
//    [128] dirSpecular         vec4  16B  xyz=dirLight.specular
//    [144] numPointLights      int    4B
//    [148] _plPad              int[3] 12B  (padding)
//    [160] plPosRadius         vec4[8] 128B  xyz=pos, w=radius
//    [288] plColorIntensity    vec4[8] 128B  xyz=color, w=intensity
//    [416] plAtten             vec4[8] 128B  x=const, y=linear, z=quad
//    [544] fogColor            vec4  16B  xyz=fog.color
//    [560] fogParams           vec4  16B  x=minDist, y=maxDist
//    [576] lightViewProj0      mat4  64B
//    [640] lightViewProj1      mat4  64B
//    [704] shadowParams        vec4  16B  x=cascadeSplit0, y=cascadeBlend, z=bias
//    [720] shadowFlags         ivec4 16B  x=shadowEnable
//  Total: 736B
//----------------------------------------------------------------------
struct GLSceneUBO
{
    float viewProj[16];              // offset   0  (64B)

    float cameraAndSun[4];           // offset  64  (16B) xyz=cameraPos, w=sunIntensity
    float ambientLight[4];           // offset  80  (16B) xyz=ambient, w=unused

    float dirDirection[4];           // offset  96  (16B) xyz=direction, w=unused
    float dirDiffuse[4];             // offset 112  (16B) xyz=diffuse,   w=unused
    float dirSpecular[4];            // offset 128  (16B) xyz=specular,  w=unused

    int   numPointLights;            // offset 144  ( 4B)
    int   _plPad[3];                 // offset 148  (12B) padding

    float plPosRadius[8][4];         // offset 160  (128B) xyz=pos, w=radius
    float plColorIntensity[8][4];    // offset 288  (128B) xyz=color, w=intensity
    float plAtten[8][4];             // offset 416  (128B) x=const, y=linear, z=quad

    float fogColor[4];               // offset 544  (16B) xyz=fog.color, w=unused
    float fogParams[4];              // offset 560  (16B) x=minDist, y=maxDist

    float lightViewProj0[16];        // offset 576  (64B)
    float lightViewProj1[16];        // offset 640  (64B)

    float shadowParams[4];           // offset 704  (16B) x=cascadeSplit0, y=cascadeBlend, z=bias
    int   shadowFlags[4];            // offset 720  (16B) x=shadowEnable
};                                   // total       736B

static_assert(sizeof(GLSceneUBO) == 736, "GLSceneUBO size mismatch — check std140 layout");

static_assert(offsetof(GLSceneUBO, viewProj)         ==   0);
static_assert(offsetof(GLSceneUBO, cameraAndSun)     ==  64);
static_assert(offsetof(GLSceneUBO, ambientLight)     ==  80);
static_assert(offsetof(GLSceneUBO, dirDirection)     ==  96);
static_assert(offsetof(GLSceneUBO, dirDiffuse)       == 112);
static_assert(offsetof(GLSceneUBO, dirSpecular)      == 128);
static_assert(offsetof(GLSceneUBO, numPointLights)   == 144);
static_assert(offsetof(GLSceneUBO, _plPad)           == 148);
static_assert(offsetof(GLSceneUBO, plPosRadius)      == 160);
static_assert(offsetof(GLSceneUBO, plColorIntensity) == 288);
static_assert(offsetof(GLSceneUBO, plAtten)          == 416);
static_assert(offsetof(GLSceneUBO, fogColor)         == 544);
static_assert(offsetof(GLSceneUBO, fogParams)        == 560);
static_assert(offsetof(GLSceneUBO, lightViewProj0)   == 576);
static_assert(offsetof(GLSceneUBO, lightViewProj1)   == 640);
static_assert(offsetof(GLSceneUBO, shadowParams)     == 704);
static_assert(offsetof(GLSceneUBO, shadowFlags)      == 720);

} // namespace toy
