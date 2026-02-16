#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

//------------------------------------------------------------
// set1: WorldCommon
// - CPU側が row-vector (v*M) / row-major 配置で詰めている前提
// - uViewProj は CPU 側で「VK clip 補正込み」にしてある想定
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

//------------------------------------------------------------
// Push constants (World + Material)  ※ frag と完全一致させる
//------------------------------------------------------------
layout(push_constant, row_major) uniform Push
{
    mat4 pcWorld;

    vec4 pcDiffuse;    // xyz = diffuse
    vec4 pcUniform;    // xyz = override color
    vec4 pcFlagsSpec;  // x=useTex, y=overrideColor, z=specPower, w=unused
} pc;

void main()
{
    // row-vector (v * M)
    vec4 worldPos = vec4(inPosition, 1.0) * pc.pcWorld;
    fragWorldPos  = worldPos.xyz;

    // uViewProj は CPU 側で VK clip 補正済み → ここで追加補正しない
    gl_Position = worldPos * sc.uViewProj;

    mat3 normalMat = mat3(transpose(inverse(pc.pcWorld)));
    fragNormal = normalize(inNormal * normalMat);

    fragTexCoord = inTexCoord;
}
