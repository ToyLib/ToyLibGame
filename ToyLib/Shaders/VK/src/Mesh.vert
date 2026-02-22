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

layout(push_constant) uniform PC
{
    mat4 world;               // 0
    vec4 baseColor_useTex;    // 64
    vec4 misc;                // 80
    vec4 overrideColor;       // 96
} pc;

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inTexCoord;

layout(location=0) out vec2 vUV;
layout(location=1) out vec3 vNormalWS;
layout(location=2) out vec3 vWorldPos;

void main()
{
    vec4 worldPos = vec4(inPosition, 1.0) * pc.world;      // ★順序は維持
    vWorldPos = worldPos.xyz;

    gl_Position = worldPos * uScene.viewProj;              // ★順序は維持

    vNormalWS = normalize(inNormal * mat3(pc.world));      // ★順序は維持
    vUV = inTexCoord;
}
