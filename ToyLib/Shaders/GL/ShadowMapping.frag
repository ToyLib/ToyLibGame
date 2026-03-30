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
//  ShadowMapping.frag
//  ライト視点の深度マップ作成用（影生成パス）
//---------------------------------------------------------------------
//  ・色情報は不要 → gl_FragDepth のみを書き込む
//  ・深度値は自動的にレンダリングされるため main() は空で良い
//  ・Phong / BasicMesh などの通常描画とは完全に別パス
//======================================================================

void main()
{
    // --------------------------------------------------------------
    // このパスでは色情報は一切不要
    // OpenGL が gl_Position から計算して gl_FragDepth を自動書き込みする
    //
    // シャドウマッピングの「深度テクスチャ生成」では以下を行う：
    //   1. 頂点シェーダーでライト空間に変換（深度値決定）
    //   2. FBO の Depth Attachment に gl_FragDepth が自動出力される
    //
    // よってフラグメント側で何も計算する必要がない。
    // --------------------------------------------------------------
}
