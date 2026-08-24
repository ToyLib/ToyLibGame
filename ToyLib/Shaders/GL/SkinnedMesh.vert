#version 410 core

//======================================================================
// ToyLib Uniform Contract (v2) - SceneUBO + BonePalette
//   C++ mirror : Render/GL/GLShaderTypes.h  (GLSceneUBO)
//   Binding    : kSceneUBOBinding=1 / kBonePaletteBinding=0
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

// ボーンパレット UBO（binding=0）
// 96 → 320 : Blender rigify 等の大規模リグ (300+ ボーン) に対応
const int kMaxPalette = 320;

layout(std140, row_major) uniform BonePalette
{
    mat4 matrixPalette[kMaxPalette];
};

//======================================================================
//  SkinnedMesh.vert
//
//  スキンメッシュ用のメイン頂点シェーダ。
//  ・ボーンパレットを使ったスキニング
//  ・ワールド変換
//  ・ビュー射影変換（カメラ空間→クリップ空間）
//  ・ライト空間座標の出力（シャドウマッピング用）
//
//  ※ ToyLib は「行ベクトル × 行列 (v * M)」で統一。
//======================================================================

// ---------------------------------------------------------
// Uniforms
// ---------------------------------------------------------

// モデル → ワールド

// ワールド → クリップ（ビューProj）

// スキニング用ボーン行列パレット
// ワールド → ライト空間（LightProj * LightView）

// ---------------------------------------------------------
// Attributes（頂点属性）
// ---------------------------------------------------------
layout(location = 0) in vec3 inPosition;    // 頂点位置
layout(location = 1) in vec3 inNormal;      // 法線
layout(location = 2) in vec2 inTexCoord;    // UV
layout(location = 3) in uvec4 inSkinBones;  // 影響ボーンID（最大4本）
layout(location = 4) in vec4  inSkinWeights;// ボーンウェイト

// ---------------------------------------------------------
// Varyings（フラグメントシェーダへ渡す値）
// ---------------------------------------------------------
out vec2 fragTexCoord;       // UV
out vec3 fragNormal;         // ワールド空間の法線
out vec3 fragWorldPos;       // ワールド座標
out vec4 fragPosLightSpace;  // ライト空間座標（シャドウマップ用）

// ---------------------------------------------------------
// メイン
// ---------------------------------------------------------
void main()
{
    // 1) 入力位置を vec4 に拡張
    vec4 pos = vec4(inPosition, 1.0);

    // 2) スキニング行列を作成（ボーン4本分の線形結合）
    mat4 skinMat =
          matrixPalette[inSkinBones[0]] * inSkinWeights[0]
        + matrixPalette[inSkinBones[1]] * inSkinWeights[1]
        + matrixPalette[inSkinBones[2]] * inSkinWeights[2]
        + matrixPalette[inSkinBones[3]] * inSkinWeights[3];

    // 3) 頂点位置のスキニング（ToyLib は v * M）
    vec4 skinnedPos = pos * skinMat;

    // 4) モデル → ワールド
    skinnedPos = skinnedPos * uObject.world;
    fragWorldPos = skinnedPos.xyz;

    // 5) ワールド → クリップ（ビュー射影）
    gl_Position = skinnedPos * uScene.viewProj;

    // 6) 法線のスキニング＆ワールド変換
    //    ※ 法線は w = 0 として扱う
    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;             // スキニング
    n = n * uObject.world;     // ワールド変換
    fragNormal = normalize(n.xyz);

    // 7) UV をそのまま転送
    fragTexCoord = inTexCoord;

}
