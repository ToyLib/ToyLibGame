#version 450

//==============================================================
// Scene (set=0)
//==============================================================
layout(set = 0, binding = 0, std140) uniform SceneUBO
{
    mat4 viewProj;
} uScene;

//==============================================================
// Push Constants (Object + Sprite)
//  - world: オブジェクト変換
//  - spriteColorAlpha: rgb=tint, a=alpha
//==============================================================
layout(push_constant) uniform PushConstants
{
    mat4 world;
    vec4 spriteColorAlpha;
} pc;

//==============================================================
// Attributes
//==============================================================
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;    // unused (kept for layout compatibility)
layout(location = 2) in vec2 inTexCoord;

//==============================================================
// Varyings
//==============================================================
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColorAlpha;

//==============================================================
// Main
//==============================================================
void main()
{
    vec4 pos = vec4(inPosition, 1.0);

    // GL版の「pos * world * viewProj」を維持（row-vector式）
    // GLSLは vec4 * mat4 も合法なのでそのまま書ける。
    gl_Position = pos * pc.world * uScene.viewProj;

    vTexCoord     = inTexCoord;
    vColorAlpha   = pc.spriteColorAlpha;
}
