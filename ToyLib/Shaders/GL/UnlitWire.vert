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
