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
// WeatherDome.vert
//
// ・スカイドーム用の頂点シェーダ
// ・aPosition は **単位球上の頂点座標**（中心原点の sky dome）
// ・そのままの方向ベクトルが「空のピクセル方向」になるため、
//   vWorldDir としてフラグメントシェーダに送る。
//   → 雲ノイズ、天の川、星などはこの方向ベクトルで計算する
//
//   (uObject.world * uScene.viewProj) = Projection * View * Model
//   → WeatherDome は常にカメラ中心で描画される想定なので、
//     モデル行列は拡大縮小のみ、位置は (0,0,0)
//======================================================================

// 頂点入力：スカイドームメッシュの位置（単位球）
layout (location = 0) in vec3 aPosition;

// MVP 行列

// フラグメントシェーダへ送る：方向ベクトル（vWorldDir）
out vec3 vWorldDir;

void main()
{
    // aPosition はスカイドームのローカル座標（＝方向）
    // スカイドームは常にカメラ中心なので transform 不要
    vWorldDir = normalize(aPosition);

    // 標準の MVP で位置をクリップ空間へ
    gl_Position = vec4(aPosition, 1.0) * (uObject.world * uScene.viewProj);
}
