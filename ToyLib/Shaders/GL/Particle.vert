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
// Particle.vert
//----------------------------------------------------------------------
// GPU パーティクル描画用の頂点シェーダ
//
// ・板ポリ（quad）をインスタンシングで描画する
// ・インスタンス属性で粒の位置(iPos)と寿命(iLife)を受け取る
// ・uCameraRight/uCameraUp を使って “常にカメラ正面” のビルボードにする
// ・寿命から vAlpha を計算し、フラグメント側で discard する
//
// 注意：行列の掛け順は ToyLib 規約に合わせている
//   gl_Position = vec4(worldPos,1) * uScene.viewProj;
//==============================================================

//==============================================================
// Vertex inputs（quad のローカル頂点）
//==============================================================
layout(location = 0) in vec3 aQuadPos;  // quad ローカル座標（-0.5..0.5）
layout(location = 1) in vec2 aUV;       // quad の UV

//==============================================================
// Instanced inputs（粒ごとのデータ）
//==============================================================
layout(location = 3) in vec3  iPos;     // 粒のワールド座標（または更新 shader の出力）
layout(location = 4) in float iLife;    // 粒の寿命（経過秒）

//==============================================================
// Uniforms
//==============================================================

// billboard basis（ワールド空間）
// カメラの右方向/上方向ベクトル（正規化済み想定）
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

uniform float uSize;           // 粒サイズ（quad のスケール）
uniform float uLifeMax;        // 粒の寿命上限（秒）

//==============================================================
// Outputs to fragment shader
//==============================================================
out vec2  vUV;                 // テクスチャ UV
out float vAlpha;              // 粒の可視度（寿命に基づくフェード結果）
out float vLife;               // デバッグ/拡張用（現状は frag で未使用でも OK）

void main()
{
    // quad の UV はそのまま転送
    vUV = aUV;

    // 現在寿命は必要ならデバッグ可視化に使える
    vLife = iLife;

    //----------------------------------------------------------
    // dead 判定
    //
    // iLife >= uLifeMax の粒は「死んだ」とみなす。
    // ・vAlpha = 0 にして frag 側で discard される
    // ・ついでにクリップ外へ飛ばして無駄な処理を減らす意図
    //
    // ※本質的には vAlpha=0 だけでも成立するが、
    //  早期に画面外へ出すことでラスタライズ負荷を下げる狙い。
    //----------------------------------------------------------
    if (iLife >= uLifeMax)
    {
        vAlpha = 0.0;
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0); // NDC で Z>1 なのでクリップ外
        return;
    }

    //----------------------------------------------------------
    // 寿命 → フェード（アルファ）計算
    //
    // t: 0..1 へ正規化した寿命割合
    // fade: 終盤(0.65..1.0)で滑らかに 1→0 へ落とす
    //----------------------------------------------------------
    float t = clamp(iLife / uLifeMax, 0.0, 1.0);
    vAlpha = 1.0 - smoothstep(0.65, 1.0, t);

    //----------------------------------------------------------
    // Billboard quad をワールド空間に展開
    //
    // aQuadPos.x/y をカメラの Right/Up ベクトル方向へ伸ばすことで
    // quad が常にカメラ正面を向く。
    //----------------------------------------------------------
    vec3 worldPos = iPos
                  + uCameraRight * (aQuadPos.x * uSize)
                  + uCameraUp    * (aQuadPos.y * uSize);

    //----------------------------------------------------------
    // クリップ空間へ
    // ToyLib convention: vec4(worldPos,1) * uScene.viewProj
    //----------------------------------------------------------
    gl_Position = vec4(worldPos, 1.0) * uScene.viewProj;
}
