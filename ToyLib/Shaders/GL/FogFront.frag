#version 410 core

//======================================================================
// ToyLib Uniform Contract (v2) - SceneUBO
//   C++ mirror : Render/GL/GLShaderTypes.h  (GLSceneUBO)
//   Binding    : Render/GL/GLBindingPoints.h (kSceneUBOBinding = 1)
//======================================================================

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

// GL 4.1 は layout(binding=N) 不可 → C++ 側で glUniformBlockBinding 設定済み
// row_major: ToyLib は v*M（行ベクトル×行列）規約
layout(std140, row_major) uniform SceneUBO
{
    mat4  viewProj;
    vec4  cameraAndSun;      // xyz=cameraPos, w=sunIntensity
    vec4  ambientLight;      // xyz=ambient
    vec4  dirDirection;      // xyz=dirLight.direction
    vec4  dirDiffuse;        // xyz=dirLight.diffuse
    vec4  dirSpecular;       // xyz=dirLight.specular
    int   numPointLights;
    int   _plPad0, _plPad1, _plPad2;
    vec4  plPosRadius[8];      // xyz=position, w=radius
    vec4  plColorIntensity[8]; // xyz=color, w=intensity
    vec4  plAtten[8];          // x=constant, y=linear, z=quadratic
    vec4  fogColor;            // xyz=fog.color
    vec4  fogParams;           // x=minDist, y=maxDist
    mat4  lightViewProj0;
    mat4  lightViewProj1;
    vec4  shadowParams;        // x=cascadeSplit0, y=cascadeBlend, z=shadowBias
    ivec4 shadowFlags;         // x=shadowEnable
} uScene;

uniform sampler2DShadow uShadowMap0;
uniform sampler2DShadow uShadowMap1;

uniform ObjectData   uObject;
uniform MaterialData uMaterial;

//======================================================================
//  FogFront.frag
//  ・画面前面に表示する「前景フォグ（白いもや）」
//  ・距離ベースのアルファ + FBMノイズによるゆらぎ
//======================================================================

out vec4 FragColor;

//======================================================================
//  Uniforms
//======================================================================

uniform float uTime;        // 時間（ノイズアニメーション用）
uniform vec2  uResolution;  // 画面サイズ（UV 正規化）
uniform float uFogAmount;   // 全体の霧量スケール

//======================================================================
//  ハッシュベースの簡易ノイズ関数
//  ・軽量で速い疑似乱数生成
//======================================================================
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

//======================================================================
//  2D Value Noise
//  ・4つの格子点から補間してノイズを生成
//======================================================================
float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    // Hermite カーブで滑らかに補間
    vec2 u = f * f * (3.0 - 2.0 * f);

    // 二次元補間
    return mix(a, b, u.x)
         + (c - a) * u.y * (1.0 - u.x)
         + (d - b) * u.x * u.y;
}

//======================================================================
//  FBM (Fractal Brownian Motion)
//  ・周波数を倍にしながら4層のノイズを合成
//  ・フォグの「揺れ」を作る
//======================================================================
float fbm(vec2 p)
{
    float value     = 0.0;
    float amplitude = 0.5;

    for (int i = 0; i < 4; ++i)
    {
        value     += amplitude * noise(p);
        p         *= 2.0;        // 周波数UP
        amplitude *= 0.5;        // 影響力DOWN
    }
    return value;
}

//======================================================================
//  main()
//======================================================================
void main()
{
    //------------------------------------------------------------------
    // Step 1 : 正規化UV（画面0〜1）
    //------------------------------------------------------------------
    vec2 uv = gl_FragCoord.xy / uResolution;

    //------------------------------------------------------------------
    // Step 2 : 中心基準の座標に変更（-0.5〜0.5）
    //          さらにアスペクト比補正で円形フォグを歪ませない
    //------------------------------------------------------------------
    vec2 centeredUV = uv - 0.5;
    centeredUV.x *= uResolution.x / uResolution.y;

    //------------------------------------------------------------------
    // Step 3 : 距離ベースのフォグ（中心 → 外側へ強くなる）
    //------------------------------------------------------------------
    float dist    = length(centeredUV);
    float baseFog = smoothstep(0.3, 0.8, dist);

    //------------------------------------------------------------------
    // Step 4 : FBM ノイズで揺れを加える
    //------------------------------------------------------------------
    float n = fbm(centeredUV * 3.5 + vec2(uTime * 0.05, 0.0));

    //------------------------------------------------------------------
    // Step 5 : フォグの強さをノイズと合成
    //------------------------------------------------------------------
    float fog = baseFog * mix(0.7, 1.0, n);

    //------------------------------------------------------------------
    // Step 6 : 出力（白いフォグ、アルファ付き）
    //------------------------------------------------------------------
    FragColor = vec4(vec3(1.0), fog * uFogAmount);
}
