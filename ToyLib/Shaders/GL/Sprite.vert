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
//  Sprite.vert
//
//  ・2Dスプライト／UI／板ポリ用の非常にシンプルな頂点シェーダ
//  ・ワールド変換 → VP変換のみ
//  ・ライト計算なし（フォグもなし）
//
//  用途：
//    - UI（ボタン・アイコン・HPバー）
//    - 画面固定のテクスチャ
//    - ゲーム内の板ポリ（フォグやライティング不要のもの）
//======================================================================

//------------------------------------------------------------------------
// Uniforms
//------------------------------------------------------------------------
// ワールド行列（平行移動・回転・スケール）

// ビュー・プロジェクション行列（カメラ）
// UI の場合は Renderer 側で画面直交行列を設定する

//------------------------------------------------------------------------
// Attributes
//------------------------------------------------------------------------
// inPosition: 頂点座標
// inNormal  : 法線（Sprite では未使用だがレイアウト合わせで残す）
// inTexCoord: UV
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;    // 未使用
layout(location = 2) in vec2 inTexCoord;

//------------------------------------------------------------------------
// フラグメントシェーダーへ渡すデータ
//------------------------------------------------------------------------
out vec2 fragTexCoord;

//======================================================================
// メイン
//======================================================================
void main()
{
    // 頂点座標を 4D ベクトルへ変換
    vec4 pos = vec4(inPosition, 1.0);

    // ワールド → カメラ → 射影
    // gl_Position は最終クリップ空間座標
    gl_Position = pos * uObject.world * uScene.viewProj;

    // UV をそのまま出力
    fragTexCoord = inTexCoord;
}
