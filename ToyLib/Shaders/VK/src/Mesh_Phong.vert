#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

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

layout(push_constant) uniform Push
{
    mat4 uWorldTransform;
} pc;

void main()
{
    vec4 worldPos = vec4(inPosition, 1.0) * pc.uWorldTransform; // v*M
    fragWorldPos = worldPos.xyz;

    gl_Position = worldPos * sc.uViewProj; // v*M

    // ★修正：行ベクトル運用に合わせる
    fragNormal   = normalize(inNormal * mat3(pc.uWorldTransform));
    fragTexCoord = inTexCoord;
}
