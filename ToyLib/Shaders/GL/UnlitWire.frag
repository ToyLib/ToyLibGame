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
//  UnlitWire.frag
//
//  ・テクスチャなしの「単色マテリアル」用フラグメントシェーダ
//  ・基本：uSolColor × uScene.ambientLight のみ
//  ・Phong や影、トゥーンは一切行わない
//
//  用途：
//   - コライダー可視化（BoundingVolume のワイヤー／ソリッド）
//   - デバッグメッシュ
//   - アイコン・簡易オブジェクト
//   - 2Dオブジェクト（UI風）にも応用可
//
//======================================================================

//-----------------------------------------------------------------------
// 頂点シェーダーから受け取る値（必要最低限）
//-----------------------------------------------------------------------
in vec3 fragNormal;     // 現状使わない（将来の拡張用）
in vec3 fragWorldPos;   // Fog 実装時などで利用する可能性あり

//-----------------------------------------------------------------------
// 出力
//-----------------------------------------------------------------------
out vec4 outColor;

//-----------------------------------------------------------------------
// Uniforms
//-----------------------------------------------------------------------
uniform vec3 uSolColor;     // 固定色（R,G,B）※アルファは常に1.0
uniform bool uUseLight;

//======================================================================
// メイン
//======================================================================
void main()
{
    vec3 col;
    if (uUseLight)
    {
        // 環境光のみの簡易ライティング
        col = uSolColor * uScene.ambientLight;
    }
    else
    {
        col = uSolColor;
    }

    outColor = vec4(col, 1.0);
}
