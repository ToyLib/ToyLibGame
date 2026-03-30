#version 410 core

//======================================================================
// ToyLib Uniform Contract (v1) - generated
//   See Render/GL/UniformNamesGL.h
//======================================================================

struct DirLight
{
    vec3 direction;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight
{
    vec3  position;
    vec3  color;
    float intensity;

    float constant;
    float linear;
    float quadratic;

    float radius;
};

struct FogInfo
{
    float maxDist;
    float minDist;
    vec3  color;
};

struct SceneData
{
    mat4 viewProj;

    vec3 cameraPos;

    vec3  ambientLight;
    float sunIntensity;

    DirLight dirLight;

    int        numPointLights;
    PointLight pointLights[8];

    FogInfo fog;

    sampler2DShadow shadowMap0;
    sampler2DShadow shadowMap1;

    mat4  lightViewProj0;
    mat4  lightViewProj1;
    float cascadeSplit0;
    float cascadeBlend;
    float shadowBias;
    int   shadowEnable;
};

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

// Max palette size must match engine-side upload
const int kMaxPalette = 96;

struct SkinnedData
{
    mat4 matrixPalette[kMaxPalette];
};

uniform SceneData    uScene;
uniform ObjectData   uObject;
uniform MaterialData uMaterial;
uniform SkinnedData  uSkinned;

//======================================================================
//  ShadowMapping_Skinned.vert
//
//  スキンメッシュ専用のシャドウマッピング用頂点シェーダ。
//  ・アニメーションのスキニング（ボーン変換）
//  ・ワールド変換
//  ・ライト空間（LightViewProj）への変換
//
//  ※色情報・法線・UV は深度パスでは使用しないため不要。
//======================================================================

// ---------------------------------------------------------
// Uniforms
// ---------------------------------------------------------

// ボーン変換行列パレット（最大96ボーン）
// モデル → ワールド変換

// ワールド → ライト空間変換（LightProj * LightView）

// ---------------------------------------------------------
// 頂点属性（頂点バッファ）
// ---------------------------------------------------------
layout(location = 0) in vec3 inPosition;     // 頂点位置
layout(location = 3) in uvec4 inSkinBones;   // 影響ボーンID（4つ）
layout(location = 4) in vec4  inSkinWeights; // ボーンウエイト（4つ）

// ---------------------------------------------------------
// メインシェーダ
// ---------------------------------------------------------
void main()
{
    // 1) スキニング処理
    vec4 pos = vec4(inPosition, 1.0);

    // 4ボーンの線形合成
    mat4 skinMat =
          uSkinned.matrixPalette[inSkinBones[0]] * inSkinWeights[0]
        + uSkinned.matrixPalette[inSkinBones[1]] * inSkinWeights[1]
        + uSkinned.matrixPalette[inSkinBones[2]] * inSkinWeights[2]
        + uSkinned.matrixPalette[inSkinBones[3]] * inSkinWeights[3];

    // スキン変換（ToyLib は 行ベクトル × 行列 ）
    vec4 skinnedPos = pos * skinMat;

    // 2) モデル → ワールド変換
    skinnedPos = skinnedPos * uObject.world;

    // 3) ワールド → ライト空間変換（これが影マップ座標）
    gl_Position = skinnedPos * uScene.lightViewProj0;

    // ※ 深度だけ使うのでフラグメント向け varyings は不要
}
