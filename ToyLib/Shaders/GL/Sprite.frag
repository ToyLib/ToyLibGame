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
//  Sprite.frag
//
//  ・2Dスプライト／UI／アイコン描画用のシンプルなフラグメントシェーダ
//  ・アルファブレンド前提（透明PNG、UI画像など）
//  ・ライティング、フォグ、影などは一切適用しない
//
//  追加：TintColor + Alpha
//   - uSpriteColor (RGB) と uSpriteAlpha (A) を掛けて色味/透明度を調整できる
//   - 背景板（白1x1）＋半透明グレーなどが簡単にできる
//
//  用途：
//   - UI 要素（ボタン、HPバー、アイコン）
//   - 2Dスプライト
//   - デバッグ用テキスト描画（ttf 以外）
//   - 画面上のオーバーレイ
//
//======================================================================

//------------------------------------------------------------------------
// 頂点シェーダーから受け取る UV 座標
//------------------------------------------------------------------------
in vec2 fragTexCoord;

//------------------------------------------------------------------------
// 出力
//------------------------------------------------------------------------
out vec4 outColor;

//------------------------------------------------------------------------
// テクスチャ（uMaterial.baseMap）
//   - Premultiplied Alpha 非対応（必要なら拡張可能）
//------------------------------------------------------------------------

//------------------------------------------------------------------------
// ★ 追加：スプライトの色（Tint）と透明度
//   - デフォルトは (1,1,1) / 1.0 を想定（=元のテクスチャそのまま）
//------------------------------------------------------------------------
uniform vec3  uSpriteColor;
uniform float uSpriteAlpha;

//======================================================================
// メイン
//======================================================================
void main()
{
    // テクスチャ取得
    vec4 tex = texture(uMaterial.baseMap, fragTexCoord);

    // RGB は Tint を乗算、Alpha はスプライト α を乗算
    //  - tex.a が元画像の透明度
    //  - uSpriteAlpha で全体フェード／半透明背景を制御
    outColor = vec4(tex.rgb * uSpriteColor, tex.a * uSpriteAlpha);
}
