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
//  Unlit.frag
//  - テクスチャ色をそのまま出す（ライト/フォグ/影は無視）
//  - ただし FootSprite/ShadowSprite 用に Tint/Alpha を追加
//  - 互換維持：uUseTint==0 のときは従来通り “テクスチャそのまま”
//
//  使い分け：
//   - TextBillboard: uUseTint を触らない（=0扱いでそのまま表示）
//   - FootSprite系 : uUseTint=1 を必ずセットして色/透明度を制御
//======================================================================

// ===== Unlit 拡張 =====
uniform int   uUseTint;       // 0: 何もしない（従来互換） / 1: tint/alpha を適用
uniform vec3  uTint;          // 乗算色（デフォルト: 1,1,1）
uniform float uAlpha;         // 乗算アルファ（デフォルト: 1）

in vec2 fragTexCoord;
out vec4 outColor;

void main()
{
    // ---- base color（まずテクスチャ）----
    vec4 base = texture(uMaterial.baseMap, fragTexCoord);

    // テクスチャ無し運用
    if (!uMaterial.useTexture)
    {
        base = vec4(uMaterial.baseColor, 1.0);
    }

    //====================================================
    // ★安全弁：
    // uUseTint 未設定でも壊れないようにする
    //====================================================
    vec3  tint  = (uUseTint != 0) ? uTint  : vec3(1.0, 1.0, 1.0);
    float alpha = (uUseTint != 0) ? uAlpha : 1.0;

    base.rgb *= tint;
    base.a   *= alpha;

    outColor = base;
}
