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
// UnlitWire.vert
// ・通常メッシュ用のベーシックな頂点シェーダ
// ・スキニングなし
// ・ワールド行列と ViewProj で位置を変換
// ・Phong や Toon など共通のフラグメントシェーダへ値を渡す
//======================================================================

//-----------------------------------------------------------------------
//  Uniforms
//-----------------------------------------------------------------------
// ワールド変換行列（モデル → ワールド）

// ビュー射影行列（ワールド → クリップ空間）

//-----------------------------------------------------------------------
//  Attributes（頂点属性）
//-----------------------------------------------------------------------
// layout(location = X) は VAO との対応
layout(location = 0) in vec3 inPosition;   // 頂点座標（ローカル）
layout(location = 1) in vec3 inNormal;     // 法線ベクトル（ローカル）
layout(location = 2) in vec2 inTexCoord;   // UV座標

//-----------------------------------------------------------------------
//  出力（Fragment Shader へ渡す）
//-----------------------------------------------------------------------
// ※視点空間ではなく「ワールド空間」で渡すのが ToyLib の基本設計
out vec2 fragTexCoord;     // UV座標
out vec3 fragNormal;       // ワールド法線
out vec3 fragWorldPos;     // 頂点のワールド座標

//-----------------------------------------------------------------------
//  main
//-----------------------------------------------------------------------
void main()
{
    // --------- ローカル → ワールド座標変換 ----------------------------
    vec4 worldPos = vec4(inPosition, 1.0) * uObject.world;

    // FSへ渡すワールド座標
    fragWorldPos = worldPos.xyz;

    // --------- ワールド法線の計算 --------------------------------------
    // mat3 でワールド行列の回転スケール部分だけ抽出し、正規化する
    fragNormal = normalize(mat3(uObject.world) * inNormal);

    // --------- クリップ空間への変換（描画用） ---------------------------
    gl_Position = worldPos * uScene.viewProj;

    // --------- UV ------------------------------------------------------
    fragTexCoord = inTexCoord;
}
