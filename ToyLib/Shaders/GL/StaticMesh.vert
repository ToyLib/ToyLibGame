#version 410

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
//  StaticMesh.vert
//  ・Phong ライティング用の標準メッシュ頂点シェーダー
//  ・シャドウマッピング用にライト空間座標も出力
//======================================================================

//======================================================================
//  Uniforms
//======================================================================

// モデル → ワールド行列

// ワールド → クリップ行列

// ワールド → ライト空間行列（シャドウマップ生成用）

//======================================================================
//  Vertex Attributes
//======================================================================

// 頂点座標
layout(location = 0) in vec3 inPosition;
// 法線ベクトル
layout(location = 1) in vec3 inNormal;
// UV（テクスチャ座標）
layout(location = 2) in vec2 inTexCoord;

//======================================================================
//  Varyings（フラグメントへ渡す）
//======================================================================

// UV
out vec2 fragTexCoord;

// ワールド空間の法線
out vec3 fragNormal;

// ワールド空間の頂点座標
out vec3 fragWorldPos;

// ライト空間座標（シャドウマップ参照用）
out vec4 fragPosLightSpace;

//======================================================================
//  main()
//======================================================================
void main()
{
    //------------------------------------------------------------------
    // Step 1 : 頂点座標をワールド空間へ
    //------------------------------------------------------------------
    vec4 worldPos = vec4(inPosition, 1.0) * uObject.world;
    fragWorldPos = worldPos.xyz;

    //------------------------------------------------------------------
    // Step 2 : ワールド座標をクリップ空間へ
    //------------------------------------------------------------------
    gl_Position = worldPos * uScene.viewProj;

    //------------------------------------------------------------------
    // Step 3 : 法線をワールド空間で変換（スケールも含める）
    //------------------------------------------------------------------
    fragNormal = normalize(inNormal * mat3(uObject.world));
    
    //------------------------------------------------------------------
    // Step 4 : UV そのまま渡す
    //------------------------------------------------------------------
    fragTexCoord = inTexCoord;

}
