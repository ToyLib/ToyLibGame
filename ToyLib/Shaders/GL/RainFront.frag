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
//  RainFront.frag
//  ・画面前面に合成する「雨ストリーク」エフェクト
//  ・スクリーンスペースで動作（カメラ位置に依存しない）
//  ・WeatherComponent の uRainAmount と組み合わせて強さ調整
//======================================================================

out vec4 FragColor;

//======================================================================
//  Uniforms
//======================================================================

// 時間（アニメーション用）
uniform float uTime;

// 画面解像度（スクリーン座標 → 正規化座標用）
uniform vec2 uResolution;

// 雨強度（0 = 無し、1 = 最大）
// WeatherComponent が時間・天候に応じて調整
uniform float uRainAmount;

//======================================================================
//  ハッシュノイズ（ランダム性の種）
//======================================================================
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(27.619, 57.583))) * 43758.5453);
}

//======================================================================
//  雨ストリーク生成
//
//  ・横方向は密度を高めるために大きくスケール
//  ・縦方向は時間でスクロールさせて「落下」
//  ・細長いハイライトの尻尾を smoothstep で作る
//======================================================================
float rainPattern(vec2 uv)
{
    // 雨の本数を増やす（横方向密度UP）
    uv *= vec2(300.0, 1.0);

    // 雨の落下速度（大きいほど高速に落ちる）
    uv.y += uTime * 8.0;

    // 柱ごとのランダムオフセット
    float id = floor(uv.x);
    float offset = hash(vec2(id, 0.0));

    // 0〜1 の範囲で縦方向ループ
    float y = fract(uv.y + offset);

    // 「細くて白い雨線」を作る
    // ・smoothstep(0,0.01) で細いハイライト
    // ・(1 - y) で下方向に向かってフェード
    float brightness = smoothstep(0.0, 0.01, y) * (1.0 - y);

    return brightness;
}

//======================================================================
//  main()
//======================================================================
void main()
{
    // スクリーンスペースUV
    vec2 uv = gl_FragCoord.xy / uResolution;

    // 雨の強度（ストリークの明るさ）
    float rain = rainPattern(uv);

    // 全体の透明度（画面前面合成用）
    // *0.25 で強すぎる雨を抑制
    float alpha = rain * uRainAmount * 0.25;

    // 雨は白色の縦線
    FragColor = vec4(vec3(1.0), alpha);
}
