#version 450

layout(location=0) in vec3 aPos;

layout(set=0, binding=0, std140, row_major) uniform ShadowSceneUBO
{
    mat4 uLightVP;
} uScene;

layout(push_constant, row_major) uniform PC
{
    mat4 uWorld;
} pc;

void main()
{
    vec4 wpos = vec4(aPos, 1.0) * pc.uWorld;
    gl_Position = wpos * uScene.uLightVP;
}
