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
          uSkinned.matrixPalette[inSkinBones[0]] * inSkinWeights[0]
        + uSkinned.matrixPalette[inSkinBones[1]] * inSkinWeights[1]
        + uSkinned.matrixPalette[inSkinBones[2]] * inSkinWeights[2]
        + uSkinned.matrixPalette[inSkinBones[3]] * inSkinWeights[3];

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
