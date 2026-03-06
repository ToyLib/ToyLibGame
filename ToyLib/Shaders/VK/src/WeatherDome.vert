#version 450

//======================================================================
// WeatherDome.vert (Vulkan)
//  - ToyLib row-vector 規約を維持
//  - gl_Position = pos * world * viewProj
//======================================================================

//--------------------------------------------------------------
// Vertex input
//--------------------------------------------------------------
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

//--------------------------------------------------------------
// Scene UBO (set=0)
//  - 既存 VKSceneUBO とレイアウトを合わせる
//--------------------------------------------------------------
struct VKPointLight
{
    vec4 position_radius;
    vec4 color_intensity;
    vec4 atten;
};

layout(std140, set = 0, binding = 0) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambient;
    vec4 dirDir;
    vec4 dirDiffuse;
    vec4 dirSpecular;

    vec4 fogColor;
    vec4 fogParams;

    ivec4 numPointLights_pad;
    VKPointLight pointLights[8];

    mat4 shadowVP0;
    mat4 shadowVP1;
    vec4 shadowParams;
} uScene;

//--------------------------------------------------------------
// Sky params UBO (set=1)
//--------------------------------------------------------------
layout(std140, set = 1, binding = 0) uniform SkyDomeParamsUBO
{
    mat4 world;

    vec4 timeParams;
    // x = uTime
    // y = uTimeOfDay
    // z = uWeatherType (floatで受けて int 化)
    // w = reserved

    vec4 sunDir;
    vec4 moonDir;
    vec4 rawSkyColor;
    vec4 rawCloudColor;
} uSky;

//--------------------------------------------------------------
// FS outputs
//--------------------------------------------------------------
layout(location = 0) out vec3 vWorldDir;

void main()
{
    vWorldDir = normalize(aPosition);

    vec4 pos = vec4(aPosition, 1.0);
    gl_Position = pos * uSky.world * uScene.viewProj;
}
