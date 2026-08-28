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

// 2D/UI パス用 VP 行列
uniform mat4 uViewProj;

//======================================================================
//  Unlit.vert
//  - 3D空間の板ポリ／看板／足場サイン用（完全Unlit）
//  - Phong/Mesh と uniform 名を揃えて互換性を維持
//  - Billboard/TextBillboard/FootSprite/ShadowSprite で共通利用可
//
//  注意：ToyLib は row-vector 前提（pos * World * ViewProj）
//======================================================================

// ===== Attributes（Sprite/Mesh 互換）=====
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;    // 未使用（互換用）
layout(location = 2) in vec2 inTexCoord;

out vec2 fragTexCoord;

void main()
{
    vec4 pos = vec4(inPosition, 1.0);
    gl_Position  = pos * uObject.world * uViewProj;
    fragTexCoord = inTexCoord;
}
