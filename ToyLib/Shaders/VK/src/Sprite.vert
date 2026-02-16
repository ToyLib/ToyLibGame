#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // unused
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

//------------------------------------------------------------
// set=1 binding=0 : SpriteCommon (viewProj)
// - row-vector (v*M)
//------------------------------------------------------------
layout(set = 1, binding = 0, std140, row_major) uniform SpriteCommon
{
    mat4 uViewProj;
} sc;

//------------------------------------------------------------
// Push constants (80 bytes)
// - mat4 world (64)
// - vec4 colorAlpha (16)
//------------------------------------------------------------
layout(push_constant, row_major) uniform Push
{
    mat4 pcWorld;
    vec4 pcColorAlpha; // unused in VS (FSで使う)
} pc;

void main()
{
    vec4 worldPos = vec4(inPosition, 1.0) * pc.pcWorld;
    gl_Position   = worldPos * sc.uViewProj;

    fragTexCoord = inTexCoord;
}
