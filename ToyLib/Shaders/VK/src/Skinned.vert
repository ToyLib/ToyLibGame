#version 450

// ---------------------------------------------------------
// set=0 : texture sampler
// ---------------------------------------------------------
layout(set = 0, binding = 0) uniform sampler2D uDiffuse;

// ---------------------------------------------------------
// set=1 : common UBO
//  - MUST match C++ struct: UBO_WorldCommon (std140)
// ---------------------------------------------------------
layout(set = 1, binding = 0, std140, row_major) uniform WorldCommon
{
    mat4  uViewProj;

    vec4  uCameraPos;
    vec4  uAmbientLight;

    float uFogMaxDist;
    float uFogMinDist;
    float _pad2_0;
    float _pad2_1;

    vec4  uFogColor;

    mat4  uLightViewProj0;
    mat4  uLightViewProj1;

    float uCascadeSplit0;
    float uCascadeBlend;
    float uShadowBias;
    int   uUseShadow;

    int   uUseToon;
    float _pad4_0;
    float _pad4_1;
    float _pad4_2;
} wc;

// ---------------------------------------------------------
// set=1 : bone palette UBO
// ---------------------------------------------------------
layout(set = 1, binding = 1, std140, row_major) uniform BonePalette
{
    mat4 uMatrixPalette[96];
} bp;

// ---------------------------------------------------------
// push constants
// ---------------------------------------------------------
layout(push_constant, row_major) uniform Push
{
    mat4 uWorldTransform;
} pc;

// ---------------------------------------------------------
// Attributes
// ---------------------------------------------------------
layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in uvec4 inSkinBones;
layout(location = 4) in vec4  inSkinWeights;

// ---------------------------------------------------------
// Varyings
// ---------------------------------------------------------
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

    // row-vector 想定: v * M
    vec4 skinnedPos = pos * skinMat;
    skinnedPos      = skinnedPos * pc.uWorldTransform;

    fragWorldPos = skinnedPos.xyz;

    gl_Position  = skinnedPos * wc.uViewProj;

    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;
    n = n * pc.uWorldTransform;
    fragNormal = normalize(n.xyz);

    fragTexCoord = inTexCoord;
}
