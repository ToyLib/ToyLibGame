#version 450

layout(location=0) in vec3  aPos;
layout(location=3) in uvec4 aBoneIds;
layout(location=4) in vec4  aWeights;

// （未使用でもOK。layoutに合わせて宣言しておくと安全）
// layout(location=1) in vec3 aNrm;
// layout(location=2) in vec2 aUV;

layout(set=0, binding=0, std140, row_major) uniform ShadowSceneUBO
{
    mat4 uLightVP;
} uScene;

layout(set=2, binding=0, std140, row_major) uniform PaletteUBO
{
    mat4 uPalette[96];
} uPal;

layout(push_constant, row_major) uniform PC
{
    mat4 uWorld;
} pc;

void main()
{
    mat4 skin =
        uPal.uPalette[aBoneIds.x] * aWeights.x +
        uPal.uPalette[aBoneIds.y] * aWeights.y +
        uPal.uPalette[aBoneIds.z] * aWeights.z +
        uPal.uPalette[aBoneIds.w] * aWeights.w;

    vec4 lpos = vec4(aPos, 1.0) * skin;
    vec4 wpos = lpos * pc.uWorld;
    gl_Position = wpos * uScene.uLightVP;
}
