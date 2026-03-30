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
//  Sprite.frag
//
//  ・2Dスプライト／UI／アイコン描画用のシンプルなフラグメントシェーダ
//  ・アルファブレンド前提（透明PNG、UI画像など）
//  ・ライティング、フォグ、影などは一切適用しない
//
//  追加：TintColor + Alpha
//   - uSpriteColor (RGB) と uSpriteAlpha (A) を掛けて色味/透明度を調整できる
//   - 背景板（白1x1）＋半透明グレーなどが簡単にできる
//
//  用途：
//   - UI 要素（ボタン、HPバー、アイコン）
//   - 2Dスプライト
//   - デバッグ用テキスト描画（ttf 以外）
//   - 画面上のオーバーレイ
//
//======================================================================

//------------------------------------------------------------------------
// 頂点シェーダーから受け取る UV 座標
//------------------------------------------------------------------------
in vec2 fragTexCoord;

//------------------------------------------------------------------------
// 出力
//------------------------------------------------------------------------
out vec4 outColor;

//------------------------------------------------------------------------
// テクスチャ（uMaterial.baseMap）
//   - Premultiplied Alpha 非対応（必要なら拡張可能）
//------------------------------------------------------------------------

//------------------------------------------------------------------------
// ★ 追加：スプライトの色（Tint）と透明度
//   - デフォルトは (1,1,1) / 1.0 を想定（=元のテクスチャそのまま）
//------------------------------------------------------------------------
uniform vec3  uSpriteColor;
uniform float uSpriteAlpha;

//======================================================================
// メイン
//======================================================================
void main()
{
    // テクスチャ取得
    vec4 tex = texture(uMaterial.baseMap, fragTexCoord);

    // RGB は Tint を乗算、Alpha はスプライト α を乗算
    //  - tex.a が元画像の透明度
    //  - uSpriteAlpha で全体フェード／半透明背景を制御
    outColor = vec4(tex.rgb * uSpriteColor, tex.a * uSpriteAlpha);
}
