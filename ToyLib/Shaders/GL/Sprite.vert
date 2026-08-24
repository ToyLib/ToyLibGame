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

// 2D/UI パス用 VP 行列（per-draw で設定、SceneUBO の 3D 透視行列とは別）
uniform mat4 uViewProj;

//======================================================================
//  Sprite.vert
//
//  ・2Dスプライト／UI／板ポリ用の非常にシンプルな頂点シェーダ
//  ・ワールド変換 → VP変換のみ
//  ・ライト計算なし（フォグもなし）
//
//  用途：
//    - UI（ボタン・アイコン・HPバー）
//    - 画面固定のテクスチャ
//    - ゲーム内の板ポリ（フォグやライティング不要のもの）
//======================================================================

//------------------------------------------------------------------------
// Uniforms
//------------------------------------------------------------------------
// ワールド行列（平行移動・回転・スケール）

// ビュー・プロジェクション行列（カメラ）
// UI の場合は Renderer 側で画面直交行列を設定する

//------------------------------------------------------------------------
// Attributes
//------------------------------------------------------------------------
// inPosition: 頂点座標
// inNormal  : 法線（Sprite では未使用だがレイアウト合わせで残す）
// inTexCoord: UV
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;    // 未使用
layout(location = 2) in vec2 inTexCoord;

//------------------------------------------------------------------------
// フラグメントシェーダーへ渡すデータ
//------------------------------------------------------------------------
out vec2 fragTexCoord;

//======================================================================
// メイン
//======================================================================
void main()
{
    // 頂点座標を 4D ベクトルへ変換
    vec4 pos = vec4(inPosition, 1.0);

    // ワールド → カメラ → 射影
    // gl_Position は最終クリップ空間座標
    gl_Position = pos * uObject.world * uViewProj;

    // UV をそのまま出力
    fragTexCoord = inTexCoord;
}
