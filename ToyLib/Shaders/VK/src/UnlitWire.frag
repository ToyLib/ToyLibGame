#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;

layout(location = 0) out vec4 outColor;

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

layout(push_constant) uniform DebugPC
{
    mat4 world;
    vec4 color;
    vec4 params; // x=useLight
} pc;

void main()
{
    vec3 col = pc.color.rgb;

    if (pc.params.x > 0.5)
    {
        col *= uScene.ambient.rgb;
    }

    outColor = vec4(col, pc.color.a);
}
