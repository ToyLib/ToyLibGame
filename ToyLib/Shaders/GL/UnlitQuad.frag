#version 410 core

//======================================================================
// ToyLib Uniform Contract (v2) - SceneUBO
//   C++ mirror : Render/GL/GLShaderTypes.h  (GLSceneUBO)
//   Binding    : Render/GL/GLBindingPoints.h (kSceneUBOBinding = 1)
//======================================================================

struct ObjectData
{
    mat4 world;
};

struct MaterialData
{
    sampler2D baseMap;

    vec3 baseColor;
    bool useTexture;

    bool toon;

    bool overrideEnabled;
    vec3 overrideColor;

    float specPower;
};

// GL 4.1 は layout(binding=N) 不可 → C++ 側で glUniformBlockBinding 設定済み
// row_major: ToyLib は v*M（行ベクトル×行列）規約
layout(std140, row_major) uniform SceneUBO
{
    mat4  viewProj;
    vec4  cameraAndSun;      // xyz=cameraPos, w=sunIntensity
    vec4  ambientLight;      // xyz=ambient
    vec4  dirDirection;      // xyz=dirLight.direction
    vec4  dirDiffuse;        // xyz=dirLight.diffuse
    vec4  dirSpecular;       // xyz=dirLight.specular
    int   numPointLights;
    int   _plPad0, _plPad1, _plPad2;
    vec4  plPosRadius[8];      // xyz=position, w=radius
    vec4  plColorIntensity[8]; // xyz=color, w=intensity
    vec4  plAtten[8];          // x=constant, y=linear, z=quadratic
    vec4  fogColor;            // xyz=fog.color
    vec4  fogParams;           // x=minDist, y=maxDist
    mat4  lightViewProj0;
    mat4  lightViewProj1;
    vec4  shadowParams;        // x=cascadeSplit0, y=cascadeBlend, z=shadowBias
    ivec4 shadowFlags;         // x=shadowEnable
} uScene;

uniform sampler2DShadow uShadowMap0;
uniform sampler2DShadow uShadowMap1;

uniform ObjectData   uObject;
uniform MaterialData uMaterial;

//======================================================================
//  Unlit.frag
//  - テクスチャ色をそのまま出す（ライト/フォグ/影は無視）
//  - ただし FootSprite/ShadowSprite 用に Tint/Alpha を追加
//  - 互換維持：uUseTint==0 のときは従来通り “テクスチャそのまま”
//
//  使い分け：
//   - TextBillboard: uUseTint を触らない（=0扱いでそのまま表示）
//   - FootSprite系 : uUseTint=1 を必ずセットして色/透明度を制御
//======================================================================

// ===== Unlit 拡張 =====
uniform int   uUseTint;       // 0: 何もしない（従来互換） / 1: tint/alpha を適用
uniform vec3  uTint;          // 乗算色（デフォルト: 1,1,1）
uniform float uAlpha;         // 乗算アルファ（デフォルト: 1）

in vec2 fragTexCoord;
out vec4 outColor;

void main()
{
    // ---- base color（まずテクスチャ）----
    vec4 base = texture(uMaterial.baseMap, fragTexCoord);

    // テクスチャ無し運用
    if (!uMaterial.useTexture)
    {
        base = vec4(uMaterial.baseColor, 1.0);
    }

    //====================================================
    // ★安全弁：
    // uUseTint 未設定でも壊れないようにする
    //====================================================
    vec3  tint  = (uUseTint != 0) ? uTint  : vec3(1.0, 1.0, 1.0);
    float alpha = (uUseTint != 0) ? uAlpha : 1.0;

    base.rgb *= tint;
    base.a   *= alpha;

    outColor = base;
}
