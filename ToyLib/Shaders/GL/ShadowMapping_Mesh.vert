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
//  ShadowMapping_Mesh.vert
//  （メッシュ専用：スキニングなし）
//
//  ライト視点の深度マップ作成パス。
//  ライト視点の座標系（LightSpaceMatrix = Projection * View）に
//  頂点を変換し、gl_Position に書き込むだけ。
//
//  このパスでは色情報を扱わず、深度値（gl_FragDepth）だけを使用。
//  フラグメントシェーダーは空で OK。
//======================================================================

// === Uniforms ===
// モデル → ワールド変換

// ワールド → ライト空間変換（LightProj * LightView）

// === 頂点属性 ===
// メッシュは深度パスでは位置のみ使用する
layout(location = 0) in vec3 inPosition;

void main()
{
    // ワールド変換 → ライト空間変換
    // gl_Position にライト空間座標を設定
    gl_Position = vec4(inPosition, 1.0) * uObject.world * uScene.lightViewProj0;

    // ※ 注意 ※
    // 深度マップでは gl_FragDepth が自動で書き込まれるため、
    // フラグメントに値を渡す必要はない。
}
