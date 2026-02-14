#version 450

layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in uvec4 inSkinBones;
layout(location = 4) in vec4  inSkinWeights;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

layout(set = 1, binding = 0, std140) uniform WorldCommon
{
    mat4 uViewProj;
    vec3 uCameraPos; float _pad0;

    vec3 uAmbientLight; float _pad1;

    float uFogMaxDist;
    float uFogMinDist;
    vec2  _pad2;
    vec3  uFogColor;
    float _pad3;

    mat4  uLightViewProj0;
    mat4  uLightViewProj1;
    float uCascadeSplit0;
    float uCascadeBlend;
    float uShadowBias;
    int   uUseShadow;
    int   uUseToon;
    float _pad4;
} sc;

layout(set = 1, binding = 6, std140) uniform SkinPalette
{
    mat4 uMatrixPalette[96];
} sk;

layout(push_constant) uniform Push
{
    mat4 uWorldTransform;
} pc;

void main()
{
    vec4 pos = vec4(inPosition, 1.0);

    mat4 skinMat =
          sk.uMatrixPalette[inSkinBones.x] * inSkinWeights.x
        + sk.uMatrixPalette[inSkinBones.y] * inSkinWeights.y
        + sk.uMatrixPalette[inSkinBones.z] * inSkinWeights.z
        + sk.uMatrixPalette[inSkinBones.w] * inSkinWeights.w;

    vec4 skinnedPos = pos * skinMat;              // v*M
    skinnedPos      = skinnedPos * pc.uWorldTransform;

    fragWorldPos = skinnedPos.xyz;
    gl_Position  = skinnedPos * sc.uViewProj;

    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;
    n = n * pc.uWorldTransform;
    fragNormal = normalize(n.xyz);

    fragTexCoord = inTexCoord;
}
