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

//==============================================================
// Particle.frag
//----------------------------------------------------------------------
// GPU パーティクル用フラグメントシェーダ
//
// ・板ポリ（quad）をインスタンシング描画
// ・粒ごとの寿命・フェード情報は頂点/インスタンス側から渡される
// ・アルファブレンド前提（加算 or 通常αは OpenGL 側で制御）
//==============================================================

//==============================================================
// Inputs（Vertex / Geometry Shader から）
//==============================================================
in vec2  vUV;      // パーティクル用テクスチャ UV
in float vAlpha;   // 粒の可視度（寿命やフェード結果）
// in float vLife;  // 現在の寿命（※デバッグや表現拡張用に残している）

//==============================================================
// Uniforms
//==============================================================

//==============================================================
// Output
//==============================================================
out vec4 outColor;

void main()
{
    //----------------------------------------------------------
    // 粒が「死んでいる」場合は描画しない
    //
    // vAlpha は頂点側で寿命やフェードを考慮して計算される想定。
    // 0 以下なら完全に不可視として discard する。
    //----------------------------------------------------------
    if (vAlpha <= 0.0)
    {
        discard;
    }

    //----------------------------------------------------------
    // テクスチャサンプリング
    //----------------------------------------------------------
    vec4 tex = texture(uMaterial.baseMap, vUV);

    //----------------------------------------------------------
    // テクスチャ自体が透明なピクセルは早期 discard
    //
    // ・不要なブレンド計算を減らす
    // ・加算合成時のゴミ描画防止
    //----------------------------------------------------------
    if (tex.a <= 0.001)
    {
        discard;
    }

    //----------------------------------------------------------
    // 最終出力カラー
    //
    // RGB  : テクスチャの色をそのまま使用
    // Alpha: テクスチャのアルファ × 粒のアルファ
    //
    // ※ ブレンド方法（加算/通常α）は OpenGL 側で設定される
    //----------------------------------------------------------
    outColor = vec4(tex.rgb, tex.a * vAlpha);
}
