#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

//------------------------------------------------------------
// set1: WorldCommon
// 重要:
// - CPU側が row-major (DirectX風 / v*M) で Matrix4 を詰めている前提なら
//   Vulkan側でも row_major を明示して “同じ並び” として解釈させる
//------------------------------------------------------------
layout(set = 1, binding = 0, std140, row_major) uniform WorldCommon
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

// Push constants: world
layout(push_constant, row_major) uniform Push
{
    mat4 uWorldTransform;
} pc;

void main()
{
    // 行ベクトル運用 (v * M)
    vec4 worldPos = vec4(inPosition, 1.0) * pc.uWorldTransform;
    fragWorldPos  = worldPos.xyz;

    vec4 clipPos  = worldPos * sc.uViewProj;

    // Vulkan クリップ空間補正:
    // - OpenGL用の投影行列をそのまま渡している場合は必須になりがち
    //   (1) Y反転
    //   (2) Z: [-1..1] -> [0..1]
    clipPos.y = -clipPos.y;
    clipPos.z = (clipPos.z + clipPos.w) * 0.5;

    gl_Position = clipPos;

    // 法線も行ベクトルに合わせる
    // ※ 非一様スケールがあるなら inverse-transpose が理想だけど、まずはこれでOK
    fragNormal   = normalize(inNormal * mat3(pc.uWorldTransform));
    fragTexCoord = inTexCoord;
}
