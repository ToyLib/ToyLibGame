#version 450

layout(set = 0, binding = 0, std140) uniform SceneUBO
{
    mat4 viewProj;
} uScene;

layout(push_constant) uniform PC
{
    mat4 world;
    vec4 colorAlpha; // rgb=tint, a=alpha
} pc;

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;    // unused
layout(location=2) in vec2 inTexCoord;

layout(location=0) out vec2 vTex;
layout(location=1) out vec4 vColorAlpha;

void main()
{
    gl_Position  = vec4(inPosition, 1.0) * pc.world * uScene.viewProj;
    vTex         = inTexCoord;
    vColorAlpha  = pc.colorAlpha;
}
