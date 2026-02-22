#version 450

layout(set = 0, binding = 0, std140) uniform SceneUBO
{
    layout(row_major) mat4 viewProj;
} uScene;

layout(push_constant) uniform PC
{
    layout(row_major) mat4 world;     // offset 0 (64 bytes)
    vec4 colorAlpha;                  // offset 64
} pc;

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;    // unused
layout(location=2) in vec2 inTexCoord;

layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vColorAlpha;

void main()
{
    // ★順番は変えない（row-vector）
    gl_Position = vec4(inPosition, 1.0) * pc.world * uScene.viewProj;

    vUV         = inTexCoord;
    vColorAlpha = pc.colorAlpha;
}
