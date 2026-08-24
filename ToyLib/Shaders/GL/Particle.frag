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

//==============================================================
// Particle.frag
//----------------------------------------------------------------------
// GPU パーティクル用フラグメントシェーダ
//
// ・板ポリ（quad）をインスタンシング描画
// ・粒ごとの寿命・フェード情報は頂点/インスタンス側から渡される
// ・アルファブレンド前提（加算 or 通常αは OpenGL 側で制御）
//==============================================================

//==============================================================
// Inputs（Vertex / Geometry Shader から）
//==============================================================
in vec2  vUV;      // パーティクル用テクスチャ UV
in float vAlpha;   // 粒の可視度（寿命やフェード結果）
// in float vLife;  // 現在の寿命（※デバッグや表現拡張用に残している）

//==============================================================
// Uniforms
//==============================================================

//==============================================================
// Output
//==============================================================
out vec4 outColor;

void main()
{
    //----------------------------------------------------------
    // 粒が「死んでいる」場合は描画しない
    //
    // vAlpha は頂点側で寿命やフェードを考慮して計算される想定。
    // 0 以下なら完全に不可視として discard する。
    //----------------------------------------------------------
    if (vAlpha <= 0.0)
    {
        discard;
    }

    //----------------------------------------------------------
    // テクスチャサンプリング
    //----------------------------------------------------------
    vec4 tex = texture(uMaterial.baseMap, vUV);

    //----------------------------------------------------------
    // テクスチャ自体が透明なピクセルは早期 discard
    //
    // ・不要なブレンド計算を減らす
    // ・加算合成時のゴミ描画防止
    //----------------------------------------------------------
    if (tex.a <= 0.001)
    {
        discard;
    }

    //----------------------------------------------------------
    // 最終出力カラー
    //
    // RGB  : テクスチャの色をそのまま使用
    // Alpha: テクスチャのアルファ × 粒のアルファ
    //
    // ※ ブレンド方法（加算/通常α）は OpenGL 側で設定される
    //----------------------------------------------------------
    outColor = vec4(tex.rgb, tex.a * vAlpha);
}
