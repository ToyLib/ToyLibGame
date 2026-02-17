#version 450

layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in uvec4 inSkinBones;
layout(location = 4) in vec4  inSkinWeights;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

//------------------------------------------------------------
// set=1 : WorldCommon（既存Meshと共通）
//  - row-vector (v*M) 前提なので row_major を付ける
//------------------------------------------------------------
layout(set = 1, binding = 0, std140, row_major) uniform WorldCommon
{
    mat4 uViewProj;
    vec3 uCameraPos;
    float _pad0;
    vec3 uAmbientLight;
    float _pad1;
    // ※ ここ以下は Mesh.frag 側と合わせて必要なら続ける
} wc;

//------------------------------------------------------------
// set=2 : Bone palette（Skinned専用）
//------------------------------------------------------------
layout(set = 2, binding = 0, std140, row_major) uniform BonePalette
{
    mat4 uMatrixPalette[96];
} bp;

//------------------------------------------------------------
// push constants（Meshと同じ：worldのみ 64 bytes）
//------------------------------------------------------------
layout(push_constant, row_major) uniform Push
{
    mat4 pcWorld;
} pc;

void main()
{
    vec4 pos = vec4(inPosition, 1.0);

    mat4 skinMat =
          bp.uMatrixPalette[inSkinBones[0]] * inSkinWeights[0]
        + bp.uMatrixPalette[inSkinBones[1]] * inSkinWeights[1]
        + bp.uMatrixPalette[inSkinBones[2]] * inSkinWeights[2]
        + bp.uMatrixPalette[inSkinBones[3]] * inSkinWeights[3];

    vec4 skinnedPos = pos * skinMat;
    skinnedPos      = skinnedPos * pc.pcWorld;

    fragWorldPos = skinnedPos.xyz;
    gl_Position  = skinnedPos * wc.uViewProj;

    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;
    n = n * pc.pcWorld;
    fragNormal = normalize(n.xyz);

    fragTexCoord = inTexCoord;
}
