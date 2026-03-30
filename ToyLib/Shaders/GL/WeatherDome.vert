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
// WeatherDome.vert
//
// ・スカイドーム用の頂点シェーダ
// ・aPosition は **単位球上の頂点座標**（中心原点の sky dome）
// ・そのままの方向ベクトルが「空のピクセル方向」になるため、
//   vWorldDir としてフラグメントシェーダに送る。
//   → 雲ノイズ、天の川、星などはこの方向ベクトルで計算する
//
//   (uObject.world * uScene.viewProj) = Projection * View * Model
//   → WeatherDome は常にカメラ中心で描画される想定なので、
//     モデル行列は拡大縮小のみ、位置は (0,0,0)
//======================================================================

// 頂点入力：スカイドームメッシュの位置（単位球）
layout (location = 0) in vec3 aPosition;

// MVP 行列

// フラグメントシェーダへ送る：方向ベクトル（vWorldDir）
out vec3 vWorldDir;

void main()
{
    // aPosition はスカイドームのローカル座標（＝方向）
    // スカイドームは常にカメラ中心なので transform 不要
    vWorldDir = normalize(aPosition);

    // 標準の MVP で位置をクリップ空間へ
    gl_Position = vec4(aPosition, 1.0) * (uObject.world * uScene.viewProj);
}
