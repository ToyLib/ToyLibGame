#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec3 vWorldDir;

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

layout(std140, set = 1, binding = 0) uniform SkyUBO
{
    vec4 time;
    vec4 weather;
    vec4 sunDir;
    vec4 moonDir;
    vec4 rawSkyColor;
    vec4 rawCloudColor;
} uSky;

layout(push_constant) uniform SkyPC
{
    mat4 world;
} pc;

void main()
{
    vWorldDir = normalize(aPosition);
    gl_Position = uScene.viewProj * pc.world * vec4(aPosition, 1.0);
}
