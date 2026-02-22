#version 450

layout(set = 0, binding = 0, std140) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;     // xyz
    vec4 ambientLight;  // xyz
    vec4 dirDir;        // xyz (direction)
    vec4 dirDiffuse;    // xyz
    vec4 dirSpecular;   // xyz
} uScene;

const int kMaxPalette = 96;

layout(set = 2, binding = 0, std140) uniform SkinnedUBO
{
    mat4 matrixPalette[kMaxPalette];
} uSkinned;

layout(push_constant) uniform PC
{
    mat4 world;               // 0
    vec4 baseColor_useTex;    // 64
    vec4 misc;                // 80
    vec4 overrideColor;       // 96
} pc;

layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in uvec4 inSkinBones;
layout(location = 4) in vec4  inSkinWeights;

layout(location=0) out vec2 vUV;
layout(location=1) out vec3 vNormalWS;
layout(location=2) out vec3 vWorldPos;

void main()
{
    vec4 pos = vec4(inPosition, 1.0);

    mat4 skinMat =
          uSkinned.matrixPalette[inSkinBones[0]] * inSkinWeights[0]
        + uSkinned.matrixPalette[inSkinBones[1]] * inSkinWeights[1]
        + uSkinned.matrixPalette[inSkinBones[2]] * inSkinWeights[2]
        + uSkinned.matrixPalette[inSkinBones[3]] * inSkinWeights[3];

    vec4 skinnedPos = pos * skinMat;        // ★順序維持
    skinnedPos = skinnedPos * pc.world;     // ★順序維持

    vWorldPos = skinnedPos.xyz;
    gl_Position = skinnedPos * uScene.viewProj; // ★順序維持

    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;                        // ★順序維持
    n = n * pc.world;                       // ★順序維持
    vNormalWS = normalize(n.xyz);

    vUV = inTexCoord;
}
