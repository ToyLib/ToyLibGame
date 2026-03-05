#version 410

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
//  StaticMesh.vert
//  ・Phong ライティング用の標準メッシュ頂点シェーダー
//  ・シャドウマッピング用にライト空間座標も出力
//======================================================================

//======================================================================
//  Uniforms
//======================================================================

// モデル → ワールド行列

// ワールド → クリップ行列

// ワールド → ライト空間行列（シャドウマップ生成用）

//======================================================================
//  Vertex Attributes
//======================================================================

// 頂点座標
layout(location = 0) in vec3 inPosition;
// 法線ベクトル
layout(location = 1) in vec3 inNormal;
// UV（テクスチャ座標）
layout(location = 2) in vec2 inTexCoord;

//======================================================================
//  Varyings（フラグメントへ渡す）
//======================================================================

// UV
out vec2 fragTexCoord;

// ワールド空間の法線
out vec3 fragNormal;

// ワールド空間の頂点座標
out vec3 fragWorldPos;

// ライト空間座標（シャドウマップ参照用）
out vec4 fragPosLightSpace;

//======================================================================
//  main()
//======================================================================
void main()
{
    //------------------------------------------------------------------
    // Step 1 : 頂点座標をワールド空間へ
    //------------------------------------------------------------------
    vec4 worldPos = vec4(inPosition, 1.0) * uObject.world;
    fragWorldPos = worldPos.xyz;

    //------------------------------------------------------------------
    // Step 2 : ワールド座標をクリップ空間へ
    //------------------------------------------------------------------
    gl_Position = worldPos * uScene.viewProj;

    //------------------------------------------------------------------
    // Step 3 : 法線をワールド空間で変換（スケールも含める）
    //------------------------------------------------------------------
    fragNormal = normalize(inNormal * mat3(uObject.world));
    
    //------------------------------------------------------------------
    // Step 4 : UV そのまま渡す
    //------------------------------------------------------------------
    fragTexCoord = inTexCoord;

}
