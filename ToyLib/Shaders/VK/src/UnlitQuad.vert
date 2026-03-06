#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;

layout(set = 0, binding = 0, std140) uniform SceneUBO
{
    mat4 viewProj;

    vec4 cameraPos;
    vec4 ambient;
    vec4 dirDir;
    vec4 dirDiffuse;
    vec4 dirSpecular;
    vec4 fogColor;
    vec4 fogParams;

    int  numPointLights;
    int  _plPad0;
    int  _plPad1;
    int  _plPad2;
} uScene;

layout(push_constant) uniform UnlitQuadPC
{
    mat4 world;
    vec4 tintAlpha;
} pc;

void main()
{
    vec4 pos = vec4(inPosition, 1.0);

    // Vulkan側はまず column-vector で合わせる
    gl_Position = uScene.viewProj * pc.world * pos;

    vTexCoord = inTexCoord;
}
