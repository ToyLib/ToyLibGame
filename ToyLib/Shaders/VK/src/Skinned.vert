#version 450

layout(set = 0, binding = 0) uniform sampler2D uDiffuse;

// set=1 : 既存MeshのWorldCommon（そのまま共有）
layout(set = 1, binding = 0, std140) uniform WorldCommon
{
    mat4 uViewProj;
    vec4 uCameraPos;
    vec4 uAmbientLight;
    float uFogMaxDist;
    float uFogMinDist;
    vec2  _pad2;
    vec4  uFogColor;
    mat4  uLightViewProj0;
    mat4  uLightViewProj1;
    vec4  uShadowParams;   // (split0, blend, bias, useShadow) みたいな詰め方でもOK
    vec4  uToonParams;     // (useToon, pad, pad, pad) でもOK
} wc;

// set=2 : BonePalette（Skinned専用）
layout(set = 2, binding = 0, std140) uniform BonePalette
{
    mat4 uMatrixPalette[96];
} bp;

layout(push_constant) uniform Push
{
    mat4 pcWorld; // ★あなたの PushConstants_Mesh に合わせる
} pc;

layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in uvec4 inSkinBones;
layout(location = 4) in vec4  inSkinWeights;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

void main()
{
    vec4 pos = vec4(inPosition, 1.0);

    mat4 skinMat =
          bp.uMatrixPalette[inSkinBones.x] * inSkinWeights.x
        + bp.uMatrixPalette[inSkinBones.y] * inSkinWeights.y
        + bp.uMatrixPalette[inSkinBones.z] * inSkinWeights.z
        + bp.uMatrixPalette[inSkinBones.w] * inSkinWeights.w;

    vec4 skinnedPos = pos * skinMat;

    skinnedPos = skinnedPos * pc.pcWorld;
    fragWorldPos = skinnedPos.xyz;

    gl_Position = skinnedPos * wc.uViewProj;

    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;
    n = n * pc.pcWorld;
    fragNormal = normalize(n.xyz);

    fragTexCoord = inTexCoord;
}
