#version 450

// ---------------------------------------------------------
// set=1 : common UBO (Meshと共有)
// ---------------------------------------------------------
layout(set = 1, binding = 0, std140, row_major) uniform WorldCommon
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
    vec4  uShadowParams; // ここは Mesh_Phong.vert と一致していればOK（例）
} wc;

// ---------------------------------------------------------
// set=2 : bone palette (Skinned専用)
// ---------------------------------------------------------
layout(set = 2, binding = 0, std140, row_major) uniform BonePalette
{
    mat4 uMatrixPalette[96];
} bp;

// ---------------------------------------------------------
// push constants（Meshと同じ PushConstants_Mesh を使う前提）
// Mesh_Phong.vert の push 定義と完全一致させること
// ---------------------------------------------------------
layout(push_constant, row_major) uniform Push
{
    mat4 pcWorld;
    vec4 pcDiffuse;
    vec4 pcUniform;
    vec4 pcFlagsSpec; // x=useTex y=override z=specPower ...
} pc;

// Attributes
layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in uvec4 inSkinBones;
layout(location = 4) in vec4  inSkinWeights;

// Varyings（Mesh_Phong.vert と一致）
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

void main()
{
    vec4 pos = vec4(inPosition, 1.0);

    // 念のため weights 正規化（データが常に正しいなら不要）
    vec4 w = inSkinWeights;
    float s = (w.x + w.y + w.z + w.w);
    if (s > 0.0) w /= s;

    // bone index clamp（保険）
    uvec4 b = min(inSkinBones, uvec4(95));

    mat4 skinMat =
          bp.uMatrixPalette[b.x] * w.x
        + bp.uMatrixPalette[b.y] * w.y
        + bp.uMatrixPalette[b.z] * w.z
        + bp.uMatrixPalette[b.w] * w.w;

    // row-vector 運用： v * M
    vec4 skinnedPos = pos * skinMat;

    // world
    vec4 wpos = skinnedPos * pc.pcWorld;
    fragWorldPos = wpos.xyz;

    // viewproj
    gl_Position = wpos * wc.uViewProj;

    // normal
    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;
    n = n * pc.pcWorld;
    fragNormal = normalize(n.xyz);

    fragTexCoord = inTexCoord;
}
