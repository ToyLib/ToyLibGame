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
    gl_Position  = pos * uObject.world * uScene.viewProj;
    fragTexCoord = inTexCoord;
}
