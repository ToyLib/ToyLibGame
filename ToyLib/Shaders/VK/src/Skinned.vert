#version 450

//============================================================
// set=0 : Material
//============================================================
layout(set = 0, binding = 0) uniform sampler2D uDiffuse;

//============================================================
// set=1 : World common / lights  （Mesh と同じ binding を維持）
//============================================================
layout(set = 1, binding = 0, std140, row_major) uniform WorldCommon
{
    mat4 uViewProj;

    vec4 uCameraPos;
    vec4 uAmbientLight;

    float uFogMaxDist;
    float uFogMinDist;
    vec2  _pad2;

    vec4 uFogColor;

    mat4 uLightViewProj0;
    mat4 uLightViewProj1;

    vec4 uShadowParams0; // (split0, blend, bias, useShadow-as-float/int) ※既存に合わせる
    vec4 uToonParams0;   // (useToon, ..., ..., ...)                    ※既存に合わせる
} wc;

// ※ Skinned.vert ではライトを使わないので宣言不要でもOK。
//   ただし「Mesh と同じ set=1 binding=1/2 が存在する」前提で
//   パイプライン側の DescriptorSetLayout は作る。
//   （frag が binding=1/2 を読むので）

//============================================================
// set=1 : Bone palette（衝突しない binding=4）
//============================================================
layout(set = 1, binding = 4, std140, row_major) uniform BonePalette
{
    mat4 uMatrixPalette[96];
} bp;

//============================================================
// push constants（Mesh と同じ）
//============================================================
layout(push_constant, row_major) uniform Push
{
    mat4 pcWorld;

    vec4 pcDiffuse;
    vec4 pcUniform;
    vec4 pcFlagsSpec;
} pc;

//============================================================
// Attributes
//============================================================
layout(location = 0) in vec3  inPosition;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in vec2  inTexCoord;
layout(location = 3) in uvec4 inSkinBones;
layout(location = 4) in vec4  inSkinWeights;

//============================================================
// Varyings（Mesh_Phong.frag に合わせる）
//============================================================
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

void main()
{
    // -----------------------------
    // normalize weights (safety)
    // -----------------------------
    vec4 w = inSkinWeights;
    float sumW = (w.x + w.y + w.z + w.w);
    if (sumW > 0.0)
    {
        w /= sumW;
    }
    else
    {
        w = vec4(1.0, 0.0, 0.0, 0.0);
    }

    // -----------------------------
    // skin matrix (row-vector)
    // -----------------------------
    mat4 skinMat =
          bp.uMatrixPalette[inSkinBones.x] * w.x
        + bp.uMatrixPalette[inSkinBones.y] * w.y
        + bp.uMatrixPalette[inSkinBones.z] * w.z
        + bp.uMatrixPalette[inSkinBones.w] * w.w;

    // position
    vec4 localPos = vec4(inPosition, 1.0);
    vec4 skinned  = localPos * skinMat;
    vec4 worldPos = skinned * pc.pcWorld;

    fragWorldPos  = worldPos.xyz;
    fragTexCoord  = inTexCoord;

    // normal (Mesh と同様に pcWorld から normalMat を作る)
    mat3 normalMat = mat3(transpose(inverse(mat3(pc.pcWorld))));

    vec3 nLocalSkinned = (vec4(inNormal, 0.0) * skinMat).xyz;
    fragNormal = normalize(nLocalSkinned * normalMat);

    // ViewProj は CPU 側で VK clip 補正済みの前提（Mesh と同じ）
    gl_Position = worldPos * wc.uViewProj;
}
